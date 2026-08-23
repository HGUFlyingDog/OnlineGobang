create database if not exists gobang;
use gobang;
create table if not exists user(
    id int primary key auto_increment,
    username varchar(20) not null unique,
    password varchar(255) not null,
    score  INT UNSIGNED default 0,
    total_count  INT UNSIGNED default 0,
    win_count  INT UNSIGNED default 0
);