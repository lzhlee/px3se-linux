#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/mm.h>
#include <asm/uaccess.h>
#include <linux/blkdev.h>
 
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <linux/major.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/seq_file.h>

#include <linux/kobject.h>
#include <linux/kobj_map.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/backing-dev.h>
#include <linux/tty.h> 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/async.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/device.h>

#include <linux/MCP23017.h> 

int count=1;
static struct i2c_client *mcp23017_client = NULL;

#define DEV_ID 0x25 				 //MCP23017 IIC设备识别码

#define I2C_READ      0x01    // ‘读’指令代码
#define I2C_WRITE     0x00    // ‘写’指令代码

#define I2C_DELAY_TIME  5			//IIC软件延时设置，根据硬件设备的速度修改此值
								    //以保证I2C_Delay()的延时时间大于4.7us
									
int I2C_SCL = 0;
int I2C_SDA = 0;

char gpio_val[4] = {0};

/*****************************************/
int mcp_iic_addr = 0;
int mcp_iic_addr0 = 0x26;
int mcp_iic_addr1 = 0x25;


static int mcp23017_i2c_write( struct i2c_client* client,uint8_t reg,uint8_t data,int device_addr)  
{  
    unsigned char buffer[2];  
    int ret;

    buffer[0] = reg;  
    buffer[1] = data;  
	
	mcp_iic_addr = mcp_iic_addr0;
	
	struct i2c_msg msgs[] = {
		{
			 .addr = mcp_iic_addr,
			 .flags = 0,
			// .len = writelen,
			.len = sizeof(buffer),
			 .buf = buffer,
		 },
	};
	client->addr=mcp_iic_addr;
	ret = i2c_transfer(client->adapter, msgs, 1);
	if (ret < 0)
	{
		dev_err(&client->dev, "%s: i2c write error.\n", __func__);
		printk("i2c write error\n");
		 return 0; 
	}

    return 0;  
} 


static int mcp23017_i2c_read( struct i2c_client* client,unsigned char reg,uint8_t *data, int device_addr)
//static int mcp23017_i2c_read( struct i2c_client* client,unsigned char reg,uint8_t *data)  
{  
	int ret;
mcp_iic_addr = mcp_iic_addr0;
	struct i2c_msg msgs[] = {
			{
				 .addr = mcp_iic_addr,
				 .flags = 0,
				// .len = 1,
				 .len = sizeof(reg),
				 .buf = &reg,// 寄存器地址
			 },
			{
				 .addr = mcp_iic_addr,
				// .flags = I2C_M_RD,0x01
				 .flags = I2C_M_RD,
				 .len = sizeof(data),
				 .buf = data,// 寄存器的值
			 },
		};

		ret = i2c_transfer(client->adapter, msgs, 2);
			if (ret < 0)
			{
				printk("i2c read error\n");
			}
		
	return ret;
		
} 


//--------------------------------------------------------------------------------------   
//  函数名称：MCP23017_IO_DIR(unsigned char deviceAddr,unsigned char port,unsigned char pin,unsigned char dir)
//  函数功能：设置制定地址的MCP23017的指定端口的指定引脚为输入或输出状态
//	参数：deviceAddr——设备地址，有A0，A1，A2决定
//		  port——端口名称，取值MCP23017_PORTA、MCP23017_PORTB
//		  pin——引脚号，取值PIN0-PIN7对应端口的8个引脚,ALLPIN包括端口所有8个引脚
//		  dir——输入输出方向，取值INPUT、OUTPUT 
//--------------------------------------------------------------------------------------   
int MCP23017_IO_DIR(struct i2c_client* client,unsigned char port,unsigned char pin,unsigned char dir,int device_addr)
{
	//unsigned char *portState;
	unsigned char portState[1];
	int res;
	
	//首先读取当前端口方向的配置状态
	//因为B端口的地址比A端口的寄存器的地址都是大1，所以采用+的技巧切换寄存器
	res = mcp23017_i2c_read(client, MCP23017_IODIR+port, &portState, device_addr);
	
	//如果出错则返回
	
	if(res == 0)
	{	
		return res;
	}
	

	if(dir==INPUT)
	{
		portState[0] |= pin;
	}
	else
	{
		portState[0] &= (~pin);
	}

	//写回方向寄存器
	res = mcp23017_i2c_write(client, MCP23017_IODIR+port, portState[0],device_addr); 
	return res;
}


//--------------------------------------------------------------------------------------   
//  函数名称：bit MCP23017_INIT(unsigned char deviceAddr,unsigned char ab,unsigned char hwa,unsigned char o)
//  函数功能：初始化指定地址的MCP23017器件
//	参数：deviceAddr——设备地址，有A0，A1，A2决定
//		  intab——配置INTA、INTB是否关联，取值INTA_INTB_CONJUNCTION、INTA_INTB_INDEPENDENT
//		  hwa——配置A0、A1、A2硬件地址是否使能，取值HWA_EN、HWA_DIS	
//		  o——配置INTA、INTB的输出类型，取值INT_OD、INT_PUSHPULL_HIGH、INT_PUSHPULL_LOW 
//  MCP23017_INIT_REG(mcp23017_client,INTA_INTB_INDEPENDENT,HWA_EN,INT_OD);
//--------------------------------------------------------------------------------------   
int MCP23017_INIT_REG( struct i2c_client* client,unsigned char intab,unsigned char hwa,unsigned char o,int device_addr)
{
	unsigned char state;
	int res;
	
	//首先设置其他位的默认状态
	state = 0x2E;		//001011 10,BANK = 0,默认不关联AB（bit = 0）,禁用顺序操作,使能变化率控制、使能硬件地址,开漏输出
	if(intab==INTA_INTB_CONJUNCTION)
	{
		state |= 0x40;
	}
	if(hwa==HWA_DIS)
	{
		state &= (~0x08);
	}
	if(o==INT_PUSHPULL_HIGH)
	{
		state &= (~0x04);
		state |= 0x02;
	}
	if(o==INT_PUSHPULL_LOW)
	{
		state &= (~0x04);
		state &= (~0x02);
	}
	
	//写回方向寄存器
	res = mcp23017_i2c_write(client, MCP23017_IOCON, state,device_addr); 
	return res;
}

//--------------------------------------------------------------------------------------   
//  函数名称：unsigned char MCP23017_READ_GPIO(unsigned char deviceAddr,unsigned char port)
//  函数功能：读取指定地址的MCP23017的指定端口值
//	参数：deviceAddr——设备地址，有A0，A1，A2决定
//		  port——端口名称，取值MCP23017_PORTA、MCP23017_PORTB
//	返回值：中断发生时，当时端口的值
//--------------------------------------------------------------------------------------   
int MCP23017_READ_GPIO(struct i2c_client* client,unsigned char port, int device_addr)
{
	uint8_t *portState;
	int res;
	
	//首先读取当前端口状态
	//因为B端口的地址比A端口的寄存器的地址都是大1，所以采用+的技巧切换寄存器  MCP23017_IODIR+port
	//res = I2C_Read_Byte_MCP23017(deviceAddr,MCP23017_GPIO+port,portState);
    //res = mcp23017_i2c_read(client, MCP23017_GPIO+port, portState, device_addr);
	
	if(res == 0)
	{
		return 0;
	}
	else
	{
		return (*portState);
	}
}

//--------------------------------------------------------------------------------------   
//  函数名称：bit MCP23017_WRITE_GPIO(unsigned char deviceAddr,unsigned char port,unsigned char val)
//  函数功能：向指定地址的MCP23017的指定端口写值
//	参数：deviceAddr——设备地址，有A0，A1，A2决定
//		  port——端口名称，取值MCP23017_PORTA、MCP23017_PORTB
//		  val——要写入端口寄存器的值
//--------------------------------------------------------------------------------------   
//int MCP23017_WRITE_GPIO(struct i2c_client* client,unsigned char port,unsigned char val)
int MCP23017_WRITE_GPIO(struct i2c_client* client,unsigned char port,unsigned char val, int device_addr)
{	
	int res;
	
	//因为B端口的地址比A端口的寄存器的地址都是大1，所以采用+的技巧切换寄存器
	res = mcp23017_i2c_write(client,MCP23017_GPIO+port,val,device_addr);

	return res;
}


static ssize_t leda_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int rec = 0;
	
	sscanf(buf, "%x", &rec);
	MCP23017_WRITE_GPIO(mcp23017_client,MCP23017_PORTA,rec,mcp_iic_addr0);
	gpio_val[0] = rec;
   return count;
}

static ssize_t leda_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	//MCP23017_READ_GPIO(mcp23017_client, MCP23017_PORTA, mcp_iic_addr0);
	//return sprintf(buf,"get_kernel_space_state:%d\n", MCP23017_READ_GPIO(mcp23017_client, MCP23017_PORTA, mcp_iic_addr0));  
	sprintf(buf, "%d\n", gpio_val[0]);
}


static ssize_t ledb_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int rec = 0;
	
	sscanf(buf, "%x", &rec);
	MCP23017_WRITE_GPIO(mcp23017_client,MCP23017_PORTB,rec,mcp_iic_addr0);
	gpio_val[1] = rec;
   return count;
}

static ssize_t ledb_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	//return sprintf(buf,"get_kernel_space_state:%d\n", MCP23017_READ_GPIO(mcp23017_client, MCP23017_PORTB, mcp_iic_addr0));  
	sprintf(buf, "%d\n", gpio_val[1]);
}

static ssize_t leda1_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int rec = 0;
	
	sscanf(buf, "%x", &rec);
	MCP23017_WRITE_GPIO(mcp23017_client,MCP23017_PORTA,rec,mcp_iic_addr1);
	gpio_val[2] = rec;
   return count;
}

static ssize_t leda1_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	//return sprintf(buf,"get_kernel_space_state:%d\n", MCP23017_READ_GPIO(mcp23017_client, MCP23017_PORTB, mcp_iic_addr0));  
	sprintf(buf, "%d\n", gpio_val[2]);
}


static ssize_t ledb1_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int rec = 0;
	
	sscanf(buf, "%x", &rec);
	MCP23017_WRITE_GPIO(mcp23017_client,MCP23017_PORTB,rec,mcp_iic_addr1);
	gpio_val[3] = rec;
   return count;
}

static ssize_t ledb1_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	//return sprintf(buf,"get_kernel_space_state:%d\n", MCP23017_READ_GPIO(mcp23017_client, MCP23017_PORTB, mcp_iic_addr0));  
	sprintf(buf, "%d\n", gpio_val[3]);
}

//static struct kobj_attribute mcp23017_attribute = __ATTR(hello_mcp23017, 0777, mcp23017_show, mcp23017_store);
static struct kobject *mcp23017_kobj;
struct  mcp23017_control_attribute {
	struct attribute	attr;
	ssize_t (*show)(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf);
	ssize_t (*store)(struct kobject *kobj, struct kobj_attribute *attr,
			const char *buf, size_t n);
};

static struct  mcp23017_control_attribute mcp23017_attribute[] = {
	//    node_name	permision		show_func	store_func 
	//__ATTR(hdmiin_test,	S_IRUGO | S_IWUSR,	hdmiin_test_show,	hdmiin_test_store),
	__ATTR(leda, 0777,	leda_show,	leda_store),
	__ATTR(ledb, 0777,	ledb_show,	ledb_store),
	
	__ATTR(leda1, 0777,	leda1_show,	leda1_store),
	__ATTR(ledb1, 0777,	ledb1_show,	ledb1_store),
	/*
	__ATTR(leda2, 0777,	NULL,	leda1_store),
	__ATTR(ledb2, 0777,	NULL,	ledb1_store),
	__ATTR(leda3, 0777,	NULL,	leda1_store),
	__ATTR(ledb3, 0777,	NULL,	ledb1_store),
	*/
};

int k =0;
static int mcp23017_probe(struct i2c_client *client, struct i2c_device_id *id)
{
	
	int i =0;
	
	printk("mcp23017_probe \n");
	printk(KERN_ALERT "Hello,mcp23017_probe \n"); 

	k++;

	for(i=0; i < 2; i++)
	{
		//sysfs_create_file(mcp23017_kobj, &mcp23017_attribute[i].attr);
		sysfs_create_file(mcp23017_kobj, &mcp23017_attribute[(k-1)*2 + i].attr);
	}
	

	/*
	for(i=0; i < 2; i++)
	{
		sysfs_create_file(mcp23017_kobj, &mcp23017_attribute[i].attr);
	}	
	*/
	
	mcp23017_client = client;
	
	//初始化MCP23017,INTA和INTB独立，使能硬件地址，INTA和INTB设置为开漏输出
	MCP23017_INIT_REG(mcp23017_client,INTA_INTB_INDEPENDENT,HWA_EN,INT_OD,mcp_iic_addr0);
	MCP23017_INIT_REG(mcp23017_client,INTA_INTB_INDEPENDENT,HWA_EN,INT_OD,mcp_iic_addr1);
		
	//将PORTA和PORTB都设置为输出
	MCP23017_IO_DIR(mcp23017_client,MCP23017_PORTA,ALLPIN,OUTPUT,mcp_iic_addr0);
	MCP23017_IO_DIR(mcp23017_client,MCP23017_PORTB,ALLPIN,OUTPUT,mcp_iic_addr0);
	MCP23017_IO_DIR(mcp23017_client,MCP23017_PORTA,ALLPIN,OUTPUT,mcp_iic_addr1);
	MCP23017_IO_DIR(mcp23017_client,MCP23017_PORTB,ALLPIN,OUTPUT,mcp_iic_addr1);
	
	MCP23017_WRITE_GPIO(mcp23017_client,MCP23017_PORTA,0x01,mcp_iic_addr0);
	MCP23017_WRITE_GPIO(mcp23017_client,MCP23017_PORTB,0x01,mcp_iic_addr0);
	MCP23017_WRITE_GPIO(mcp23017_client,MCP23017_PORTA,0x01,mcp_iic_addr1);
	MCP23017_WRITE_GPIO(mcp23017_client,MCP23017_PORTB,0x01,mcp_iic_addr1);
}


static const struct i2c_device_id mcp23017_id[] = {
	//{"mcp23017", 0},
	{"mcp23017_A", 0},
	{"mcp23017_B", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, mcp23017_id);

/*
static const struct of_device_id of_rk_mcp23017_match[] = {
	//{.compatible = "mcp23017" },
	//{.compatible = "mcp23017_PA" },
	//{.compatible = "mcp23017_PB" },
};
*/

static struct i2c_driver mcp23017_drv = { 
    .driver     = { 
        .name   = "mcp23017",
        .owner  = THIS_MODULE,
        //.of_match_table = of_match_ptr(of_rk_mcp23017_match),
    },  
    .probe      = mcp23017_probe,
	.id_table       = mcp23017_id,
};

static int mcp23017_init(void)
{
    	
	//mcp23017_kobj = kobject_create_and_add("hello_mcp23017", kernel_kobj);
	mcp23017_kobj = kobject_create_and_add("hello_mcp23017", NULL);
	i2c_add_driver(&mcp23017_drv);
	
    return 0;
}

static void mcp23017_exit(void)
{
    i2c_del_driver(&mcp23017_drv);
	k =0;
}

module_init(mcp23017_init);
module_exit(mcp23017_exit);
MODULE_LICENSE("GPL");
