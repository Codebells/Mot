# Cypher Query Language

图数据库保存了数据之间的关系。将数据像有(无)向图一样存储。

​	Neo4j自己设计的Cypher Query Language来查询Neo4j图数据库



# Nebula graph图数据库

![[外链图片转存失败,源站可能有防盗链机制,建议将图片保存下来直接上传(img-cSmfOiZz-1608195445199)(http://images.coderstudy.vip/架构图)]](https://img-blog.csdnimg.cn/ca16609c1eee4b92b300f51e5aec3093.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0NTRE5fX19MWVk=,size_16,color_FFFFFF,t_70)

存算分离架构

#### Query Service

查询引擎部分，提供查询服务；

主要功能就是解析nGQL，生成优化后的执行计划，组成action算子链提交执行！

Graph query engine和sql查询引擎基本一样，除了解析以及最后执行的实际操作不一样

先解析成语法树，再生成执行计划，再优化执行计划，再执行

#### Storage Service

存储引擎部分，基于RocksDb开发的分布式底层存储，用来持久化存储图数据；

#### Meta Service

元数据管理服务，主要用于图库集群元数据的管理；

注意，如果该服务不可用，会导致整体图库的不可用，所以建议集群部署，提高可用性

