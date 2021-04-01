1、修改package.sh文件中target_file_copy函数下的370行，如图所示： 详情请参看文档【腾讯文档】Gauss编译打包过程优化https://docs.qq.com/doc/DZlNsS0NPSHFGUXdh

而后将目录“优化后的脚本”下面的两个脚本替换到对应目录下，之后，仅需要按照原先的步骤编译打包即可。 

2、开启编译，打包过程。且，等待这个过程结束后，注释掉370行的这行代码
3、运行以下命令：
mkdir /root/software_resources/openGauss-server/temp_files/
cp -r /root/software_resources/openGauss-server/dest/temp/lib /root/software_resources/openGauss-server/temp_files/

cp -r /root/software_resources/openGauss-server/dest/temp/include /root/software_resources/openGauss-server/temp_files/
而后将build.sh 和package.sh分别覆盖到对应位置（直接替换源文件也可）
运行rm -rf /root/software_resources/openGauss-server/dest/
至此，优化完成。

之后，我们之前进行安装编译的所有步骤，照常进行。
亲测
编译：52s
安装：53s
打包：105s
合计：3分30秒
