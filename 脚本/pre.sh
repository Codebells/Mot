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

#安装
su – omm

gs_install -X /tmp/clusterconfig.xml


#想要重装的时候，请执行以下操作“
#su – omm

gs_uninstall --delete-data
userdel -r omm

另外/opt/huawei和/opt/software这两个目录删掉
/root/gs_om删掉。

然后重新执行45-64行就没有问题了。
经过多次试验






[GAUSS-51400] : Failed to execute the command: python3 '/opt/software/openGauss/script/local/PreInstallUtility.py' -t create_os_user -u omm -g dbgrp -l /var/log/omm/omm/om/gs_local.log. Result:{'gaussdb.sugon.com': 'Failure'}.
Error:
[FAILURE] gaussdb.sugon.com:
'getpwuid(): uid not found: 1000'










