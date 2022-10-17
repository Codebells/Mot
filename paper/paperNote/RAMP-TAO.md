---
title: VLDB 2014 RAMP_TAO论文阅读
date: 2022-05-28 18:42:06
categories:
  - [database]
  - [paper_read]
tags: [paper_read,database]
category_bar: true
excerpt: 阅读RAMP_TAO Layering Atomic Transactions on Facebook’s Online TAO Data Store笔记，接着上篇RAMP的思路继续。
---

# RAMP-TAO

提出RAMP协议变体RAMP-TAO在TAO中以最小的开销部署，同时确保大规模读取优化工作负载的原子可见性。

基本思想，只需要保证最近的读不违反原子读，事务性的更新数据来减少原子可见性的开销

## RAMP-TAO变体

### RefillLibrary

![TAO cache示意图](./ramp-tao/image-20220527191318356.png)

一个源信息缓冲池，缓冲了最近3分钟的所有region的写，muti-version

![RefillLibrary示意图](./ramp-tao/image-20220527165757240.png)

### Algorithm

第3-5，大部分的TAO的读，都是已经符合原子读的。7-21在检查是否符合原子读。使用RefillLibrary来存储原子性检查需要的信息。

![RAMP-TAO 伪代码](./ramp-tao/image-20220527165211747.png)

![TAO事务流程](./ramp-tao/image-20220527170858839.png)

