/*
make wifi_client
sudo ./wifi_client

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#define NETLINK_WIFI_SERVER 30
#define MSG_TYPE_QUERY_RSSI 1
#define MSG_TYPE_RESP_RSSI 2

#define MAX_PAYLOAD 1024  // 定义最大载荷长度

int main() {
    int sock_fd;
    struct sockaddr_nl src_addr, dest_addr;
    struct nlmsghdr *nlh = NULL;
    struct iovec iov;
    struct msghdr msg;
    int ret;

    // 1. 创建Netlink套接字（指定正确的协议号）
    sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_WIFI_SERVER);
    if (sock_fd < 0) {
        perror("socket create failed");
        return -1;
    }

    // 2. 初始化源地址（当前进程PID）
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid();  // 客户端进程PID
    src_addr.nl_groups = 0;

    // 3. 绑定套接字（必须绑定，否则内核无法回包）
    if (bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
        perror("bind failed");
        close(sock_fd);
        return -1;
    }

    // 4. 初始化目标地址（内核进程，PID=0）
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0;  // 内核的PID固定为0
    dest_addr.nl_groups = 0;

    // 5. 分配Netlink消息缓冲区
    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    if (!nlh) {
        perror("malloc failed");
        close(sock_fd);
        return -1;
    }
    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));

    // 6. 填充Netlink消息头
    nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
    nlh->nlmsg_pid = getpid();  // 客户端PID，供内核回包使用
    nlh->nlmsg_type = MSG_TYPE_QUERY_RSSI;  // 查询类型
    nlh->nlmsg_flags = NLM_F_REQUEST;       // 标记为请求消息

    // 7. 填充消息数据（可选，仅标识查询）
    strcpy((char*)NLMSG_DATA(nlh), "query_rssi");

    // 8. 构建msghdr结构体
    iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;
    msg.msg_name = (void *)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = 0;

    // 9. 发送消息到内核
    printf("Sending RSSI query to kernel (PID: %d)...\n", getpid());
    ret = sendmsg(sock_fd, &msg, 0);
    if (ret < 0) {
        perror("sendmsg failed");
        free(nlh);
        close(sock_fd);
        return -1;
    }

    // 10. 接收内核响应（阻塞等待）
    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
    printf("Waiting for kernel response...\n");
    ret = recvmsg(sock_fd, &msg, 0);
    if (ret < 0) {
        perror("recvmsg failed");
        free(nlh);
        close(sock_fd);
        return -1;
    }

    // 11. 解析并打印响应（关键：强制转换为char*消除警告）
    printf("Received response from kernel: %s\n", (char*)NLMSG_DATA(nlh));

    // 12. 清理资源
    free(nlh);
    close(sock_fd);
    return 0;
}
