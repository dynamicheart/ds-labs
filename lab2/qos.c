#include <stdlib.h>

#include "rte_common.h"
#include "rte_meter.h"
#include "rte_red.h"
#include "rte_cycles.h"

#include "qos.h"

/**
 * srTCM
 */

/* 每个参数的说明以及作用：
 *  cir:
 *      - 承诺访问速率,每秒钟往C桶和E桶填充新令牌的速率,一个令牌相当于一个Byte
 *      - 单位 Byte/s
 *  cbs:
 *     - C桶容量
 *     - 增大的话,承受burst的能力增强
 *     - 单位 Byte
 *  ebs:
 *     - E桶容量
 *     - 增大的话,承受burst的能力增强
 *     - 单位 Byte
 */

// 1. 虚拟机CPU的hz为3,095,221,586
// 2. cir_period指的是每隔多少个cycles填充一次令牌桶,CIR bytes per period 指的是每个period填充多少个bytes
// 3. 通过计算main中发包速率,得出每隔1,000,000个cycles,平均每一个流要发(1000/4)Packets * 640 Bytes = 160,000,
//    即每秒每个包要发送495,235,453.76Bytes
// 4. 对于FLOW 0,要让其获得最大带宽,则其可能的最大发包速率为(128+1024)*1500 = 1,728,000 Byte,cbs 和 ebs应该设得尽可能大,使其获得绿包。
// 5. 对于FLOW 1，其cir应为FLOW 0的一半,其它流同理
static struct rte_meter_srtcm_params app_srtcm_params[APP_FLOWS_MAX] = {
     {.cir = 498235435,  .cbs = 10000 * 36, .ebs = 10000 * 48}, // Flow 0
     {.cir = 249117717, .cbs = 10000 * 7, .ebs = 10000 * 36}, // Flow 1
     {.cir = 124558858, .cbs = 1000 * 32, .ebs = 10000 * 20}, // Flow 2
     {.cir = 62279429, .cbs = 1000 * 16, .ebs = 10000 * 16} // Flow 3
 };

static struct rte_meter_srtcm app_flows[APP_FLOWS_MAX];

// 
static uint64_t start_times[APP_FLOWS_MAX];

int
qos_meter_init(void)
{
    // Configure flow table
    uint32_t i;
    int ret;

    // Log the hz of current cpu
    printf("QoS Menter: hz = %lu\n", rte_get_tsc_hz());

    for (i = 0; i < APP_FLOWS_MAX; i++) {
        ret = rte_meter_srtcm_config(&app_flows[i], &app_srtcm_params[i]);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "Invalid configure flow table\n");

        start_times[i] = rte_get_tsc_cycles();
    }

    return 0;
}

enum qos_color
qos_meter_run(uint32_t flow_id, uint32_t pkt_len, uint64_t time)
{
    return (enum qos_color)rte_meter_srtcm_color_blind_check(&app_flows[flow_id], start_times[flow_id] + time, pkt_len);
}


/**
 * WRED
 */

/*
 * 每个参数的说明以及作用：
 *  min_th:
 *      - 小队列长度,当队列小于该长度时,不会丢包,在min和max之间开始丢包，丢包可能性随q增大而增大，最大丢包可能性为maxp
 *  max_th:
 *      - 最大队列长度,当队列大于该长度时,丢包率为100%
 *  maxp_inv:
 *      - 队列长度在min和max之间时最大的丢包可能性, 10表示, 10个包中有1个包会丢
 *  wq_log2:
 *      - 决定平均队列长度变化速率的快慢,同一种流的wq_log2的值要相同。
 */

static struct rte_red_params app_red_params[APP_FLOWS_MAX][e_RTE_METER_COLORS] = {
    { // Flow 0
        {.min_th = 1022, .max_th = 1023, .maxp_inv = 255, .wq_log2 = 9}, // Green
        {.min_th = 1022, .max_th = 1023, .maxp_inv = 255, .wq_log2 = 9}, // Yellow
        {.min_th = 1022, .max_th = 1023, .maxp_inv = 255, .wq_log2 = 9}  // Red
    },
    { // Flow 1
        {.min_th = 64, .max_th = 1023, .maxp_inv = 10, .wq_log2 = 9}, // Green
        {.min_th = 16, .max_th = 32, .maxp_inv = 10, .wq_log2 = 9}, // Yellow
        {.min_th = 1, .max_th = 16, .maxp_inv = 10, .wq_log2 = 9}  // Red
    },
    { // Flow 2
        {.min_th = 64, .max_th = 1023, .maxp_inv = 10, .wq_log2 = 9}, // Green
        {.min_th = 4, .max_th = 16, .maxp_inv = 6, .wq_log2 = 9}, // Yellow
        {.min_th = 1, .max_th = 8, .maxp_inv = 3, .wq_log2 = 9}  // Red
    },
    { // Flow 3
        {.min_th = 64, .max_th = 1023, .maxp_inv = 10, .wq_log2 = 9}, // Green
        {.min_th = 2, .max_th = 6, .maxp_inv = 3, .wq_log2 = 9}, // Yellow
        {.min_th = 1, .max_th = 3, .maxp_inv = 2, .wq_log2 = 9}  // Red
    }
};

static struct rte_red_config app_red_configs[APP_FLOWS_MAX][e_RTE_METER_COLORS];

// Run-time data
static struct rte_red app_reds[APP_FLOWS_MAX][e_RTE_METER_COLORS];

// Queue size in packets
unsigned queues[APP_FLOWS_MAX];

// Last time of burst
uint64_t last_burst_time;

int
qos_dropper_init(void)
{
    uint32_t i, j;
    int ret;

    for (i = 0; i < APP_FLOWS_MAX; i++) {
        for (j = 0; j < e_RTE_METER_COLORS; j++) {
            // Initialize red run-time data
            ret = rte_red_rt_data_init(&app_reds[i][j]);
            if (ret < 0)
                rte_exit(EXIT_FAILURE, "Failed to initialize red run-time datas\n");

            // Configure RED configuration
            ret = rte_red_config_init(&app_red_configs[i][j],
                app_red_params[i][j].wq_log2,
                app_red_params[i][j].min_th,
                app_red_params[i][j].max_th,
                app_red_params[i][j].maxp_inv
            );
            if (ret < 0)
                rte_exit(EXIT_FAILURE, "Invalid red params for flow %d :color %d, error code:%d\n", i + 1, j, ret);
        }

        // Initialize queue
        queues[i] = 0;
    }

    last_burst_time = 0;

    return 0;
}

int
qos_dropper_run(uint32_t flow_id, enum qos_color color, uint64_t time)
{
    // Send out all packets in queues
    if (time != last_burst_time) {
        for (int i = 0; i < APP_FLOWS_MAX; i++)
            queues[i] = 0;
        last_burst_time = time;
    }

    if (rte_red_enqueue(&app_red_configs[flow_id][color], &app_reds[flow_id][color], queues[flow_id], start_times[flow_id] + time) == 0) {
        queues[flow_id]++;
        return 0;
    }

    return 1;
}
