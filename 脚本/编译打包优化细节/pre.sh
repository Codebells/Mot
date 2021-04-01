#配置
yum install -y libaio-devel flex bison ncurses-devel glibc-devel patch redhat-lsb readline-devel bzip2
yum install -y python36
yum install -y bzip2 libaio-devel flex bison ncurses-devel glibc-devel patch
yum install -y net-tools tar lrzsz
systemctl disable firewalld.service
systemctl stop firewalld.service
sed -i 's/^SELINUX=.*/SELINUX=disabled/' /etc/selinux/config
setenforce 0
if [ "$LANG" != "en_US.UTF-8" ];then
export LANG=en_US.UTF-8
echo export LANG=en_US.UTF-8 >> /etc/profile
fi
ln -sf /usr/share/zoneinfo/Asia/Shanghai /etc/localtime
swapoff -a
ifconfig
ifconfig ens192 mtu 8192
sed -i 's/^Banner .*/Banner none/' /etc/selinux/config
sed -i 's/^#PermitRootLogin .*/PermitRootLogin yes/' /etc/selinux/config
sed -i 's/^PermitRootLogin no/PermitRootLogin yes/' /etc/selinux/config
systemctl restart sshd
echo 5 > /proc/sys/net/ipv4/tcp_retries1
echo 5 > /proc/sys/net/ipv4/tcp_syn_retries
echo 10 > /proc/sys/net/sctp/path_max_retrans
echo 10 > /proc/sys/net/sctp/max_init_retransmits
echo net.ipv4.tcp_retries1 = 5 >>/etc/sysctl.conf
echo net.ipv4.tcp_syn_retries = 5 >>/etc/sysctl.conf
echo net.sctp.path_max_retrans = 10 >>/etc/sysctl.conf
echo net.sctp.max_init_retransmits = 10 >>/etc/sysctl.conf
yum install -y ntp
systemctl start ntpd && systemctl enable ntpd

#编译
mkdir /root/software_resources/
cd /root/software_resources/
unzip 1.0.0.zip
tar -zxvf openGauss-third_party_binarylibs.tar.gz
mv openGauss-third_party_binarylibs binarylibs
mv  binarylibs /root/software_resources/openGauss-server/
cd /root/software_resources/openGauss-server/
sh build.sh -m debug -pkg



#预安装
userdel -r omm
rm -rf /opt/huawei
rm -rf /opt/software
rm -rf /root/gauss_om


mkdir /opt/huawei
chmod 777 /opt/huawei
mkdir -p /opt/software/openGauss
chmod 777 -R /opt/software
cd /opt/software/openGauss
cd /root/software_resources/openGauss-server/package
\cp -rf openGauss-1.0.0-CentOS-64bit.tar.gz /opt/software/openGauss
cd /opt/software/openGauss
cd /root/software_resources/
rm -rf /tmp/clusterconfig.xml
cp  clusterconfig.xml /tmp/
cd /opt/software/openGauss/
tar -zxvf openGauss-1.0.0-CentOS-64bit.tar.gz
cd /opt/software/openGauss/
rm -rf version.cfg
cd /root/software_resources/
cp  version.cfg /opt/software/openGauss/
export LD_LIBRARY_PATH=/opt/software/openGauss/script/gspylib/clib:$LD_LIBRARY_PATH
cd /opt/software/openGauss/script
./gs_preinstall -U omm -G dbgrp -X /tmp/clusterconfig.xml





hostnamectl set-hostname gaussdb.sugon.com
wget  https://gitee.com/opengauss/openGauss-server/repository/archive/1.0.0.zip

wget https://opengauss.obs.cn-south-1.myhuaweicloud.com/1.0.0/openGauss-third_party_binarylibs.tar.gz







MOT::RC res = m_engine->GetTableManager()->GetTable("postgres_public_cyl6")->InsertRow(localRow, remote_txn);


#安装
su - omm
gs_install -X /tmp/clusterconfig.xml


userdel -r omm
rm -rf /opt/huawei
rm -rf /opt/software
rm -rf /root/gauss_om

vi /opt/huawei/install/data/db1/postgresql.conf
enable_incremental_checkpoint = on
checkpoint部分需要改为off
开启关闭服务：gs_om -t start/stop
进入用户：su -omm 
进入数据库：gsql -d postgres -p 26000
退出数据库：ctrl+d
退出用户：exit

//查看gs_ctl日志
gs_ctl start -D opt/huawei/data/db1
cd /var/log/omm/omm/bin/gs_ctl


/opt/huawei/install/app/bin

3、查看pg_log过程，
/var/log/omm/omm/pg_log/dn_6001，选择最新的，查看动态的尾部日志
tail -f 对应的log文件名称
ALTER ROLE omm IDENTIFIED BY 'pengqi.1998' REPLACE 'Pengqi1998';
create foreign table cyl666 (i int,j int );
insert into cyl666 values (1,1);
insert into cyl666 values (2,2);
insert into cyl666 values (3,3);
insert into cyl666 values (4,4);
insert into cyl666 values (6,6);
insert into cyl666 values (7,7);
insert into cyl666 values (8,8);
insert into cyl666 values (9,9);
insert into cyl666 values (10,10);
insert into cyl666 values (1,91);
insert into cyl666 values (92,92);
insert into cyl666 values (93,93);
insert into cyl666 values (94,94);
insert into cyl666 values (95,95);
insert into cyl666 values (96,96);

insert into cyl666 values (111,111);
insert into cyl666 values (222,222);
insert into cyl666 values (333,333);
insert into cyl666 values (444,444);
insert into cyl666 values (666,666);
insert into cyl666 values (777,777);
insert into cyl666 values (888,888);
insert into cyl666 values (999,999);

C:\Users\Administrator/Desktop/openGauss-server-本地E/openGauss-server-本地E/src/gausskernel/storage/mot/fdw_adapter/src/mot_internal.cpp

/root/software_resources/openGauss-server/src/gausskernel/storage/mot/fdw_adapter/src/mot_internal.cpp

ALTER ROLE omm IDENTIFIED BY 'pengqi.1998' REPLACE 'Pengqi1998';
create foreign table cyl6 (i int,j int );
insert into cyl6 values (1,1);
insert into cyl6 values (2,2);
insert into cyl6 values (3,3);
insert into cyl6 values (4,4);
insert into cyl6 values (6,6);
insert into cyl6 values (7,7);
insert into cyl6 values (8,8);
insert into cyl6 values (9,9);
insert into cyl6 values (10,10);
insert into cyl6 values (1,91);
insert into cyl6 values (92,92);
insert into cyl6 values (93,93);
insert into cyl6 values (94,94);
insert into cyl6 values (95,95);
insert into cyl6 values (96,96);




create foreign table cyl666(i int primary key,j int);
insert into cyl666 values (1,1);
insert into cyl666 values (2,2);
insert into cyl666 values (3,3);
insert into cyl666 values (4,4);
insert into cy3 values (5);
insert into cyl666 values (6,6);
insert into cyl666 values (7,7);
insert into cyl666 values (8,8);
insert into cyl666 values (9,9);
insert into cyl666 values (10,10);
insert into cyl666 values (1,91);
insert into cyl666 values (92,92);
insert into cyl666 values (93,93);
insert into cyl666 values (94,94);
insert into cyl666 values (95,95);
insert into cyl666 values (96,96);
update cyl666  set j=999 where i=111 ;
update cyl666  set j=999 where i=222 ;
delete from cyl666 where i=14;
delete from cyl666 where i=222;
create foreign table cyl4(i int,j int);
insert into cyl4 values (1,1);
insert into cyl4 values (2,2);
insert into cyl4 values (3,3);
insert into cyl4 values (4,4);

gs_guc set -N all -p 26000 -I all -h "host all jack 202.199.6.60/32 sha256"
update cyl666  set j=9998 where i=111 ;
update cyl666  set j=888888 where i=222 ;

insert into cyl666 values (6,966666664);

create foreign table cyl666(i int,j int);
insert into cyl666 values (1,1);
insert into cyl666 values (2,2);
insert into cyl666 values (3,3);
insert into cyl666 values (4,4);
insert into cy3 values (5);
insert into cyl666 values (6,6);
insert into cyl666 values (7,7);
insert into cyl666 values (8,8);
insert into cyl666 values (9,9);
insert into cyl666 values (10,10);
insert into cyl666 values (1,91);
insert into cyl666 values (92,92);
insert into cyl666 values (93,93);
insert into cyl666 values (94,94);
insert into cyl666 values (95,95);
insert into cyl666 values (96,96);






tbreak /root/software_resources/openGauss-server/src/gausskernel/storage/mot/fdw_adapter/src/mot_internal.cpp:  1091



#想要重装的时候，请执行以下操作“
#su – omm

gs_uninstall --delete-data
为保险起见，可以先reboot下，只需要等待15秒即可，再进行后续操作
reboot
userdel -r omm
rm -rf /opt/huawei
rm -rf /opt/software
rm -rf /root/gauss_om
另外/opt/huawei、/opt/software和/root/gs_om这三个目录删掉
rm -rf /opt/huawei
rm -rf /opt/software
rm -rf /root/gauss_om



然后重新执行45-64行就没有问题了。
经过多次试验


{
    "name":"gaussdb.sugon.com",
    "host":"119.3.175.195",
    "port":22,
    "username":"root",
    "password":"Pengqi1998",
    "protocol":"sftp",
    "remotePath":"/root/software_resources/openGauss-server",
    "uploadOnSave":true
}

{
    "name":"gaussdb.sugon.com",
    "host":"209.216.86.209",
    "port":5001,
    "username":"root",
    "password":"zwx",
    "protocol":"sftp",
    "remotePath":"/root/software_resources/openGauss-server",
    "uploadOnSave":true
}



[GAUSS-51400] : Failed to execute the command: python3 '/opt/software/openGauss/script/local/PreInstallUtility.py' -t create_os_user -u omm -g dbgrp -l /var/log/omm/omm/om/gs_local.log. Result:{'gaussdb.sugon.com': 'Failure'}.
Error:
[FAILURE] gaussdb.sugon.com:
'getpwuid(): uid not found: 1000'





E:\document\final_opengauss\openGauss-server\package\package.sh


export CODE_BASE=/root/openGauss-server    # Path of the openGauss-server file
export BINARYLIBS=$CODE_BASE/../binarylibs    # Path of the binarylibs file
export GAUSSHOME=/opt/opengauss/
export GCC_PATH=$BINARYLIBS/buildtools/centos7.6_x86_64/gcc8.2
export CC=$GCC_PATH/gcc/bin/gcc
export CXX=$GCC_PATH/gcc/bin/g++
export LD_LIBRARY_PATH=$GAUSSHOME/lib:$GCC_PATH/gcc/lib64:$GCC_PATH/isl/lib:$GCC_PATH/mpc/lib/:$GCC_PATH/mpfr/lib/:$GCC_PATH/gmp/lib/:$LD_LIBRARY_PATH
export PATH=$GAUSSHOME/bin:$GCC_PATH/gcc/bin:$PATH


./configure --gcc-version=8.2.0 CC=g++ CFLAGS='-O0' --prefix=$GAUSSHOME --3rd=$BINARYLIBS --enable-debug --enable-cassert --enable-thread-safety --without-readline --without-zlib



switch (thread_role) {



mkdir /opt/huawei
chmod 777 /opt/huawei
mkdir -p /opt/software/openGauss
chmod 777 -R /opt/software
cd /opt/software/openGauss
cd /root/software_resources/pq/openGauss-server/package
\cp -rf openGauss-1.0.0-CentOS-64bit.tar.gz /opt/software/openGauss
cd /opt/software/openGauss
cd /root/software_resources/
rm -rf /tmp/clusterconfig.xml
cp  clusterconfig.xml /tmp/
cd /opt/software/openGauss/
tar -zxvf openGauss-1.0.0-CentOS-64bit.tar.gz
cd /opt/software/openGauss/
rm -rf version.cfg
cd /root/software_resources/
cp  version.cfg /opt/software/openGauss/
export LD_LIBRARY_PATH=/opt/software/openGauss/script/gspylib/clib:$LD_LIBRARY_PATH
cd /opt/software/openGauss/script
./gs_preinstall -U omm -G dbgrp -X /tmp/clusterconfig.xml





安装ZeroMQ Protobuf lib
yum install -y autoconf automake  libtool curl  make g++ unzip

wget https://github.com/jedisct1/libsodium/releases/download/1.0.3/libsodium-1.0.3.tar.gz
 tar -xvzf libsodium-1.0.3.tar.gz
 cd libsodium-1.0.3
 ./configure
 make
 make install
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig 
ldconfig


 tar -xvzf zeromq-4.1.3.tar.gz
 cd zeromq-4.1.3
 ./configure
 make
 make install
ldconfig



cd protobuf

./autogen.sh
yum groupinstall Development tools -y 
//预编译到/usr/protubuf 目录下
./configure --prefix=/usr/protobuf   

//安装
make && make install



https://www.cnblogs.com/q1359720840/p/14122920.html









