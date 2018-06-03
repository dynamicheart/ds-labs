import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.io.DoubleWritable;
import org.apache.hadoop.io.IntWritable;
import org.apache.hadoop.io.Text;
import org.apache.hadoop.mapreduce.Job;
import org.apache.hadoop.mapreduce.Mapper;
import org.apache.hadoop.mapreduce.Reducer;
import org.apache.hadoop.mapreduce.lib.input.MultipleInputs;
import org.apache.hadoop.mapreduce.lib.input.TextInputFormat;
import org.apache.hadoop.mapreduce.lib.output.FileOutputFormat;

import java.io.IOException;

public class DValueSumApp {

    private final static String DELIMITER = ",";
    private final static String NULL_SYMBOL = "\\N";
    private final static String DEVICE_TABLE_TAG = "DEV";
    private final static String DVALUES_TABLE_TAG = "DVAL";

    public static class DeviceJoinMapper
            extends Mapper<Object, Text, IntWritable, Text> {

        private String[] fields;
        private IntWritable outputKey = new IntWritable(0);
        private Text outputValue = new Text();

        @Override
        protected void map(Object key, Text value, Context context) throws IOException, InterruptedException {
            fields = value.toString().split(DELIMITER);
            // Key format is id
            outputKey.set(Integer.valueOf(fields[0]));
            // Value format is DEV,type
            outputValue.set(DEVICE_TABLE_TAG + DELIMITER + fields[1]);
            context.write(outputKey, outputValue);
        }
    }

    public static class DValueJoinMapper
            extends Mapper<Object, Text, IntWritable, Text> {

        private String[] fields;
        private IntWritable outputKey = new IntWritable();
        private Text outputValue = new Text();

        @Override
        protected void map(Object key, Text value, Context context) throws IOException, InterruptedException {
            fields = value.toString().split(DELIMITER);
            Integer did = Integer.valueOf(fields[0]);
            // Filter out irrelevant records
            if (did > 0 && did < 1000 && fields[1].equals(NULL_SYMBOL)) {
                // Key format is did
                outputKey.set(did);
                // Value format is DVAL,value
                outputValue.set(DVALUES_TABLE_TAG + DELIMITER + fields[2]);
                context.write(outputKey, outputValue);
            }
        }
    }

    public static class JoinReducer
            extends Reducer<IntWritable, Text, Text, DoubleWritable> {
        private String[] fields;
        private String[] deviceFields;
        private String[] dvaluesFields;

        private Text outputKey = new Text();
        private DoubleWritable outputValue = new DoubleWritable();

        @Override
        protected void reduce(IntWritable key, Iterable<Text> values, Context context) throws IOException, InterruptedException {
            deviceFields = null;
            dvaluesFields = null;
            for(Text value: values) {
                fields = value.toString().split(DELIMITER);
                if(fields[0].equals(DEVICE_TABLE_TAG)) {
                    deviceFields = fields;
                }
                else {
                    dvaluesFields = fields;
                }
            }

            if (deviceFields != null && dvaluesFields != null) {
                outputKey.set(deviceFields[1]);
                outputValue.set(Double.valueOf(dvaluesFields[1]));
                context.write(outputKey, outputValue);
            } else {
                System.out.printf("Device %d does not join\n", key.get());
            }
        }
    }


    public static class GroupMapper
            extends Mapper<Text, DoubleWritable, Text, DoubleWritable> {
        @Override
        protected void map(Text key, DoubleWritable value, Context context) throws IOException, InterruptedException {
            context.write(key, value);
        }
    }

    public static class SumReducer
            extends Reducer<Text, DoubleWritable, Text, DoubleWritable> {
        private DoubleWritable result = new DoubleWritable();

        @Override
        protected void reduce(Text key, Iterable<DoubleWritable> values, Context context) throws IOException, InterruptedException {
            double sum = 0;
            for(DoubleWritable value: values) {
                sum += value.get();
            }
            result.set(sum);
            context.write(key, result);
        }
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 3) {
            System.err.println("Usages: DValueSumApp <device.txt path> <dvalues.txt path> <output path>");
            System.exit(-1);
        }

        Configuration joinJobConf = new Configuration();
        Job joinJob = Job.getInstance(joinJobConf, "Device and DValue Join");
        joinJob.setJarByClass(DValueSumApp.class);
        MultipleInputs.addInputPath(joinJob, new Path(args[0]), TextInputFormat.class, DeviceJoinMapper.class);
        MultipleInputs.addInputPath(joinJob, new Path(args[1]), TextInputFormat.class, DValueJoinMapper.class);
        joinJob.setMapOutputKeyClass(IntWritable.class);
        joinJob.setMapOutputValueClass(Text.class);
        joinJob.setReducerClass(JoinReducer.class);
        joinJob.setOutputKeyClass(Text.class);
        joinJob.setOutputValueClass(DoubleWritable.class);
        FileOutputFormat.setOutputPath(joinJob, new Path(args[2]));
        System.exit(joinJob.waitForCompletion(true) ? 0 : 1);
    }
}
