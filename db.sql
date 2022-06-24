drop database if exists `cloud_drive`;
create database `cloud_drive`;
use `cloud_drive`;

create table `sessions` (
    `sessionid` varchar(128) not null,
    `expires` int(11) not null,
    `data` mediumtext,
    primary key (`sessionid`)
);

create table `user` (
    `userid` char(16) not null,
    `username` char(16) not null unique,
    `passwd` varchar(64) not null,
    primary key (`userid`)
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
    constraint fk_login_user
        foreign key (`userid`) references `user`(`userid`)
        on delete cascade on update cascade
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

insert into `user`(`username`, `passwd`) values('test', sha2('123', 256));
