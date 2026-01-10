#include <linux/init.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <net/sock.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/jiffies.h>

#define NETLINK_WIFI_SERVER 30
#define MSG_TYPE_QUERY_RSSI 1
#define MSG_TYPE_RESP_RSSI 2
static struct sock *nl_wifi_sock = NULL;

struct wifi_dev {
    int irq;
    void __iomem *regs;
    int rssi;
    struct tasklet_struct irq_tasklet;
};
static struct wifi_dev *wifi_dev = NULL;

static void wifi_irq_tasklet_func(unsigned long data) {
    struct wifi_dev *dev = (struct wifi_dev *)data;
    if (!dev) return;
    dev->rssi = -45 + (jiffies % 10);
    pr_info("WiFi irq: updated RSSI to %d dBm\n", dev->rssi);
}

static irqreturn_t wifi_irq_handler(int irq, void *dev_id) {
    return IRQ_HANDLED;
}

// 修复后的Netlink消息处理函数
static void nl_wifi_server_handle_msg(struct sk_buff *skb) {
    struct nlmsghdr *nlh = NULL;
    char resp_buf[64] = {0};
    struct sk_buff *resp_skb;
    u32 client_pid;  // 客户端PID（关键：从消息头获取）

    if (!skb || !wifi_dev) {
        pr_err("WiFi server: invalid skb or wifi_dev\n");
        return;
    }

    // 解析客户端消息头
    nlh = nlmsg_hdr(skb);
    if (!nlh) {
        pr_err("WiFi server: invalid nlmsg header\n");
        return;
    }
    client_pid = nlh->nlmsg_pid;  // 获取客户端PID（核心！）
    pr_info("WiFi server: received msg from user PID: %u, type: %u\n",
            client_pid, nlh->nlmsg_type);

    if (nlh->nlmsg_type == MSG_TYPE_QUERY_RSSI) {
        snprintf(resp_buf, sizeof(resp_buf), "WiFi RSSI: %d dBm", wifi_dev->rssi);
        pr_info("WiFi server: prepare response: %s\n", resp_buf);

        // 1. 分配响应skb（严格按数据长度计算，避免冗余）
        resp_skb = nlmsg_new(strlen(resp_buf) + 1, GFP_KERNEL);
        if (!resp_skb) {
            pr_err("WiFi server: allocate skb failed\n");
            return;
        }

        // 2. 填充响应消息头（关键：nlmsg_pid设为客户端PID）
        nlh = nlmsg_put(resp_skb, 
                        client_pid,  // 目标PID（客户端）
                        0,           // 序列号（0即可）
                        MSG_TYPE_RESP_RSSI, 
                        strlen(resp_buf) + 1,  // 数据长度（含'\0'）
                        0);          // 标志位
        if (!nlh) {
            pr_err("WiFi server: put nlmsg failed\n");
            nlmsg_free(resp_skb);
            return;
        }

        // 3. 填充响应数据
        strcpy(NLMSG_DATA(nlh), resp_buf);

        // 4. 发送响应（核心：用client_pid作为目标PID）
        if (nlmsg_unicast(nl_wifi_sock, resp_skb, client_pid) < 0) {
            pr_err("WiFi server: nlmsg_unicast failed\n");
            nlmsg_free(resp_skb);
        } else {
            pr_info("WiFi server: sent response to user PID %u: %s\n",
                    client_pid, resp_buf);
        }
    }
}

static int __init wifi_driver_init(void) {
    int ret;
    struct netlink_kernel_cfg cfg = {
        .input = nl_wifi_server_handle_msg,
        .groups = 0,
        .flags = NL_CFG_F_NONROOT_RECV,
    };

    pr_info("WiFi driver: start initialization (simulation mode)\n");

    wifi_dev = kzalloc(sizeof(struct wifi_dev), GFP_KERNEL);
    if (!wifi_dev) {
        pr_err("WiFi driver: kzalloc wifi_dev failed\n");
        return -ENOMEM;
    }
    wifi_dev->rssi = -50;
    pr_info("WiFi driver: allocate wifi_dev success\n");

    pr_info("WiFi driver: skip ioremap (simulation mode)\n");
    pr_info("WiFi driver: skip request_irq (simulation mode)\n");

    tasklet_init(&wifi_dev->irq_tasklet,
                 wifi_irq_tasklet_func,
                 (unsigned long)wifi_dev);
    pr_info("WiFi driver: tasklet init success\n");

    // 创建Netlink套接字（显式指定NETLINK_GENERIC，兼容部分内核）
    nl_wifi_sock = netlink_kernel_create(&init_net, NETLINK_WIFI_SERVER, &cfg);
    if (!nl_wifi_sock) {
        pr_err("WiFi driver: create netlink socket failed\n");
        kfree(wifi_dev);
        return -ENOMEM;
    }
    pr_info("WiFi driver: netlink socket create success (protocol: %d)\n", NETLINK_WIFI_SERVER);

    tasklet_schedule(&wifi_dev->irq_tasklet);

    pr_info("WiFi driver initialized successfully (simulation mode)\n");
    return 0;
}

static void __exit wifi_driver_exit(void) {
    pr_info("WiFi driver: start unload (simulation mode)\n");

    if (wifi_dev) {
        tasklet_kill(&wifi_dev->irq_tasklet);
        pr_info("WiFi driver: tasklet killed\n");
    }

    if (nl_wifi_sock) {
        netlink_kernel_release(nl_wifi_sock);
        pr_info("WiFi driver: netlink socket released\n");
    }

    if (wifi_dev) {
        kfree(wifi_dev);
        wifi_dev = NULL;
        pr_info("WiFi driver: wifi_dev freed\n");
    }

    pr_info("WiFi driver unloaded successfully (simulation mode)\n");
}

module_init(wifi_driver_init);
module_exit(wifi_driver_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("WiFi Driver (Netlink + Tasklet - Simulation Mode)");
MODULE_AUTHOR("Alex");
