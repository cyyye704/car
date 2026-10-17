/*********************************************************************************************************************
* STC32G144K Opensourec Library 即（STC32G144K 开源库）是一个基于官方 SDK 接口的第三方开源库
* Copyright (c) 2025 SEEKFREE 逐飞科技
*
* 本文件是STC32G144K开源库的一部分
*
* STC32G144K 开源库 是免费软件
* 您可以根据自由软件基金会发布的 GPL（GNU General Public License，即 GNU通用公共许可证）的条款
* 即 GPL 的第3版（即 GPL3.0）或（您选择的）任何后来的版本，重新发布和/或修改它
*
* 本开源库的发布是希望它能发挥作用，但并未对其作任何的保证
* 甚至没有隐含的适销性或适合特定用途的保证
* 更多细节请参见 GPL
*
* 您应该在收到本开源库的同时收到一份 GPL 的副本
* 如果没有，请参阅<https://www.gnu.org/licenses/>
*
* 额外注明：
* 本开源库使用 GPL3.0 开源许可证协议 以上许可申明为译文版本
* 许可申明英文版在 libraries/doc 文件夹下的 GPL3_permission_statement.txt 文件中
* 许可证副本在 libraries 文件夹下 即该文件夹下的 LICENSE 文件
* 欢迎各位使用并传播本程序 但修改内容时必须保留逐飞科技的版权声明（即本声明）
*
* 文件名称          
* 公司名称          成都逐飞科技有限公司
* 版本信息          查看 libraries/doc 文件夹内 version 文件 版本说明
* 开发环境          MDK FOR C251
* 适用平台          STC32G144K
* 店铺链接          https://seekfree.taobao.com/
*
* 修改记录
* 日期              作者           备注
* 2025-11-20        大W            first version
********************************************************************************************************************/

#include "zf_common_headfile.h"

// *************************** 例程硬件连接说明 ***************************
// 使用 type-c 连接
//      直接将type-c插入核心板
//
// *************************** 例程测试说明 ***************************
// 1.核心板烧录完成本例程，单独使用 type-c接口 在断电情况下完成连接
//
// 2.将 type-c接口模块连接电脑，完成上电
//
// 3.电脑上使用串口助手打开对应的串口
//
// 4.可以在串口助手上看到如下串口信息：
//      usb_cdc Text.
//
// 5.通过串口助手发送数据，会收到相同的反馈数据
//      usb_cdc get data:.......
//
// 如果发现现象与说明严重不符 请参照本文件最下方 例程常见问题说明 进行排查

uint32  fifo_data_count;
uint8   fifo_get_data[DEBUG_RING_BUFFER_LEN] = {0};            // fifo 输出读出缓冲区

void main(void)
{
    clock_init(SYSTEM_CLOCK_96M); 				// 时钟配置及系统初始化<务必保留>
    debug_init();                       		// 调试串口信息初始化

    system_delay_ms(1000);
    
	usb_cdc_write_string("usb_cdc Text.");      // 输出测试信息
    usb_cdc_write_byte('\r');                   // 输出回车
    usb_cdc_write_byte('\n');                   // 输出换行

	
	// 此处编写用户代码 例如外设初始化代码等
	
	while(1)
	{
        // 此处编写需要循环执行的代码
        fifo_data_count = fifo_used(&debug_uart_fifo);                          // 查看 fifo 是否有数据
        if(fifo_data_count != 0)                                                // 读取到数据了
        {
            // 为了防止在读取FIFO的时候，又写入FIFO，这里关闭总中断。
            interrupt_global_disable();
            fifo_read_buffer(&debug_uart_fifo, fifo_get_data, &fifo_data_count, FIFO_READ_AND_CLEAN);    // 将 fifo 中数据读出并清空 fifo 挂载的缓冲
            interrupt_global_enable();
            
            usb_cdc_write_string("usb_cdc get data:");                          // 输出测试信息
            usb_cdc_write_buffer(fifo_get_data, (uint16)fifo_data_count);       // 将读取到的数据发送出去
            usb_cdc_write_string("\r\n");                                       // 输出测试信息
        }

        system_delay_ms(10);
        // 此处编写需要循环执行的代码
	}
}

// *************************** 例程常见问题说明 ***************************
// 遇到问题时请按照以下问题检查列表检查
//
// 问题1：串口没有数据
//      查看串口助手打开的是否是正确的串口，检查打开的 COM 口是否对应的是板载的type-c
//      如果是使用板载的type-c连接，那么检查下载器线是否松动
