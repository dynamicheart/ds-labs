select type,sum(value) from device, dvalues
where id = did and did > 0 and did < 1000 and date is null
group by type order by type desc;

select date, type, avg(value) from device left join dvalues on id = did
where did > 0 and did < 10 and date id not null
group by date, type order by date desc, type;

select * from device into outfile '/var/lib/mysql-files/device.txt'
fields terminated by ','
optionally enclosed by ''
lines terminated by '\n';

select * from dvalues into outfile '/var/lib/mysql-files/dvalues.txt'
fields terminated by ','
optionally enclosed by ''
lines terminated by '\n';

