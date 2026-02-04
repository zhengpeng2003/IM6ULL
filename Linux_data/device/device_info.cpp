#include "device_info.hpp"

#include <sys/utsname.h>
#include <cstdio>
#include <cstring>
#include <iostream>
#include "data_protocol.h"
#include "data_packer.h"
#include "ipc_server.h"
using namespace std;
/* ================= 内部工具函数 ================= */

static void get_kernel_info(char *kernel, size_t klen,
                            char *arch,   size_t alen)
{
    struct utsname u;
    if (uname(&u) != 0)
        return;

    strncpy(kernel, u.release, klen - 1);
    kernel[klen - 1] = '\0';

    strncpy(arch, u.machine, alen - 1);
    arch[alen - 1] = '\0';
}

static void get_os_info(char *os, size_t len)
{
    FILE *fp = fopen("/etc/os-release", "r");
    if (!fp)
        return;

    char line[256];

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
            char *p = strchr(line, '=');
            if (!p)
                break;

            p++;                // skip '='
            if (*p == '"') p++; // skip leading quote

            char *end = strchr(p, '"');
            if (end) *end = '\0';

            strncpy(os, p, len - 1);
            os[len - 1] = '\0';
            break;
        }
    }

    fclose(fp);
}

static void get_fb_resolution(int *w, int *h)
{
    if (!w || !h)
        return;

    FILE *fp = fopen("/sys/class/graphics/fb0/virtual_size", "r");
    if (!fp)
        return;

    fscanf(fp, "%d,%d", w, h);
    fclose(fp);
}

/* ================= 对外接口 ================= */

void Deviceinfo_send()
{
    device_data_t dev;
    memset(&dev, 0, sizeof(dev));

    dev.device_id = 0;
    dev.type  = DEV_SYSINFO;
    dev.valid = 1;

    get_kernel_info(dev.data.sys.kernel,
                    sizeof(dev.data.sys.kernel),
                    dev.data.sys.arch,
                    sizeof(dev.data.sys.arch));

    get_os_info(dev.data.sys.os,
                sizeof(dev.data.sys.os));

    get_fb_resolution(&dev.data.sys.screen_w,
                      &dev.data.sys.screen_h);

    data_pack_t pack = data_pack_single(&dev);

    char json[512];
    int len = data_pack_to_json(&pack, json, sizeof(json));
    if (len > 0) {
	   cout<<"数据已经发送"<<" "<<json<<endl;
        ipc_server_send(json);
    }
}

