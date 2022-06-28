drop database if exists `cloud_drive`;
create database `cloud_drive`;
use `cloud_drive`;

create table `sessions` (
    `sessionid` varchar(128) not null,
    `expires` int(11) not null,
    `data` mediumtext,
    primary key (`sessionid`)
);

create table `invcode` (
    `invcode` char(8) not null,
    `maxnum` int default 10,
    primary key (`invcode`)
);

create table `user` (
    `userid` char(16) not null,
    `username` varchar(64) not null unique,
    `passwd` char(64) not null, -- SHA256
    `invcode` char(8) not null,
    primary key (`userid`),
    constraint fk_user_invcode
        foreign key (`invcode`) references `invcode`(`invcode`)
        on delete cascade on update cascade
);

create table `last` (
    `userid` char(16) not null,
    primary key (`userid`),
    constraint fk_last_user
        foreign key (`userid`) references `user`(`userid`)
        on delete cascade on update cascade
);

create table `login` (
    `sessionid` varchar(128) not null,
    `userid` char(16) not null,
    primary key (`sessionid`),
    constraint fk_login_user
        foreign key (`userid`) references `user`(`userid`)
        on delete cascade on update cascade
);

create table `upload` (
    `fileid` char(128) not null, -- SHA512
    `begin` bigint not null,
    `end` bigint not null,
    `status` varchar(16) not null default 'pending',
    primary key (`fileid`, `begin`, `end`),
    check (`end` > `begin`),
    check (`status` = 'pending' or
        `status` = 'uploading')
);

delimiter -;
create trigger `before_user_insert`
    before insert on `user`
    for each row
begin
    declare `sel` char(16);
    repeat
        set new.`userid` = substr(md5(rand()), 1, 16);
        set `sel` = (
            select count(*) from `user`
            where `user`.`userid` = new.`userid`);
    until `sel` = 0 end repeat;
end-;

create trigger `after_user_insert`
    after insert on `user`
    for each row
begin
    if (select count(*) from `last`) != 1 then
        delete from `last`;
        insert into `last` values(new.`userid`);
    else
        update `last` set `userid` = new.`userid`;
    end if;
end-;
delimiter ;

insert into `invcode` values ('00000000', 2);
-- insert into `user`(`username`, `passwd`, `invcode`) values('test', sha2('123', 256), '00000000');
insert into `user`(`username`, `passwd`, `invcode`) values('test', '123', '00000000');
