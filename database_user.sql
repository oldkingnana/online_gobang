drop table Online_Gobang.user;

create database Online_Gobang;

create table Online_Gobang.user
(
	uid int AUTO_INCREMENT PRIMARY KEY, 
	usr_name VARCHAR(64), 
	passwd VARCHAR(64), 
	game_cnt int UNSIGNED, 
	win_cnt int UNSIGNED, 
	los_cnt int UNSIGNED, 
	point int
);

show databases;

show create table Online_Gobang.user;


