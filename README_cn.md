中文|[英文](README.md)

# Road Segmentation

本Application支持运行在Atlas 200 DK上，实现了路面实时分割的功能。

<p align='center'>
    <img src='img/img3.jpg' height=300>
</p>

<p align='center'>
    <img src='to_out/test.png'>
</p>

<p align='center'>
    <img src='img/img4.png'>
</p>

## 前提条件

部署此Sample前，需要准备好以下环境：

-   已完成Mind Studio的安装。
-   已完成Atlas 200 DK开发者板与Mind Studio的连接，交叉编译器的安装，SD卡的制作及基本信息的配置等。

## 软件准备

运行此Sample前，需要按照此章节获取源码包，并进行相关的环境配置。

1. 获取源码包。

   将[https://github.com/Ascend/road-segmentation](https://github.com/Ascend/road-segmentation)仓中的代码以Mind Studio安装用户下载至Mind Studio所在Ubuntu服务器的任意目录，例如代码存放路径为：_/home/ascend/road-segmentation_。

2.  以Mind Studio安装用户登录Mind Studio所在Ubuntu服务器，并设置环境变量DDK\_HOME。

    **vim \~/.bashrc**

    执行如下命令在最后一行添加DDK\_HOME及LD\_LIBRARY\_PATH的环境变量。

    **export DDK\_HOME=/home/XXX/tools/che/ddk/ddk**

    **export LD\_LIBRARY\_PATH=$DDK\_HOME/uihost/lib**

    >**说明：**   
    >-   XXX为Mind Studio安装用户，/home/XXX/tools为DDK默认安装路径。  
    >-   如果此环境变量已经添加，则此步骤可跳过。  

    输入:wq!保存退出。

    执行如下命令使环境变量生效。

    **source \~/.bashrc**


## 部署

1. 以Mind Studio安装用户进入路面分割应用代码所在根目录，如**_/home/ascend/road-segmentation_**。

2.  执行部署脚本，进行工程环境准备，包括公共库的编译与部署、应用的编译与部署等操作。

    bash deploy.sh  _host\_ip_ _model\_mode_

    -   _host\_ip_：对于Atlas 200 DK开发者板，即为开发者板的IP地址。
    -   local：若Mind Studio所在Ubuntu系统未连接网络，请使用local模式，执行此命令前，需要参考[公共代码库下载](#zh-cn_topic_0182554604_section92241245122511)将依赖的公共代码库ezdvpp下载到“sample-objectdetection/script“目录下。
    -   internet：若Mind Studio所在Ubuntu系统已连接网络，请使用internet模式，在线下载依赖代码库ezdvpp。

    命令示例：

    **bash deploy.sh 192.168.1.2 internet**

3. 将需要使用的已经转换好的Davinci离线模型文件上传至Host侧_~/HIAI\_PROJECTS/ascend\_workspace/segmentation/out_目录下。

   ```bash
   scp kittisegRealTime.om HwHiAiUser@host_ip:/home/HwHiAiUser/HIAI_PROJECTS/ascend_workspace/segmentation/out/kittisegRealTime.om
   ```

   对于Atlas 200 DK，host\_ip默认为192.168.1.2（USB连接）或者192.168.0.2（NIC连接）。


## 运行

1.  在Mind Studio所在Ubuntu服务器中，执行

    ```bash
    python3 run_server.py
    ```

2.  在Mind Studio所在Ubuntu服务器中，以HwHiAiUser用户SSH登录到Host侧。

    ```bash
    ssh HwHiAiUser@host_ip
    ```

    对于Atlas 200 DK，host\_ip默认为192.168.1.2（USB连接）或者192.168.0.2（NIC连接）。

3. 进入路面分割网络应用的可执行文件所在路径。

   ```bash
   cd ~/HIAI_PROJECTS/ascend_workspace/segmentation/out
   ```

4. 执行应用程序

   使用相机获取图片
   
   ```bash
   ./ascend_segmentation
   ```
   ![image2](img/img2.png)
   
   使用测试图片
   
   ```bash
   ./ascend_segmentation 1
   ```
   ![image1](img/img1.png)
   
   - 输入图片宽度：623px。
   - 输入图片高度：188px。


## 公共代码库下载<a name="zh-cn_topic_0182554604_section92241245122511"></a>

将依赖的软件库下载到“road-segmentation/script“目录下。

**表 2**  依赖代码库下载

<table><thead align="left"><tr id="zh-cn_topic_0182554604_row3576111214511"><th class="cellrowborder" valign="top" width="33.33333333333333%" id="mcps1.2.4.1.1"><p id="zh-cn_topic_0182554604_p5576712114510"><a name="zh-cn_topic_0182554604_p5576712114510"></a><a name="zh-cn_topic_0182554604_p5576712114510"></a>模块名称</p>
</th>
<th class="cellrowborder" valign="top" width="33.33333333333333%" id="mcps1.2.4.1.2"><p id="zh-cn_topic_0182554604_p157661218455"><a name="zh-cn_topic_0182554604_p157661218455"></a><a name="zh-cn_topic_0182554604_p157661218455"></a>模块描述</p>
</th>
<th class="cellrowborder" valign="top" width="33.33333333333333%" id="mcps1.2.4.1.3"><p id="zh-cn_topic_0182554604_p10576201211454"><a name="zh-cn_topic_0182554604_p10576201211454"></a><a name="zh-cn_topic_0182554604_p10576201211454"></a>下载地址</p>
</th>
</tr>
</thead>
<tbody><tr id="zh-cn_topic_0182554604_row1757621219458"><td class="cellrowborder" valign="top" width="33.33333333333333%" headers="mcps1.2.4.1.1 "><p id="zh-cn_topic_0182554604_p15576212114511"><a name="zh-cn_topic_0182554604_p15576212114511"></a><a name="zh-cn_topic_0182554604_p15576212114511"></a>EZDVPP</p>
</td>
<td class="cellrowborder" valign="top" width="33.33333333333333%" headers="mcps1.2.4.1.2 "><p id="zh-cn_topic_0182554604_p1257661204510"><a name="zh-cn_topic_0182554604_p1257661204510"></a><a name="zh-cn_topic_0182554604_p1257661204510"></a>对DVPP接口进行了封装，提供对图片/视频的处理能力。</p>
</td>
<td class="cellrowborder" valign="top" width="33.33333333333333%" headers="mcps1.2.4.1.3 "><p id="zh-cn_topic_0182554604_p11576312114515"><a name="zh-cn_topic_0182554604_p11576312114515"></a><a name="zh-cn_topic_0182554604_p11576312114515"></a><a href="https://github.com/Ascend/sdk-ezdvpp" target="_blank" rel="noopener noreferrer">https://github.com/Ascend/sdk-ezdvpp</a></p>
<p id="zh-cn_topic_0182554604_p18576131264519"><a name="zh-cn_topic_0182554604_p18576131264519"></a><a name="zh-cn_topic_0182554604_p18576131264519"></a>下载后请保持文件夹名称为ezdvpp。</p>
</td>
</tr>
</tbody>
</table>

