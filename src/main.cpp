//
//  main.cpp
//  futurerestore
//
//  Created by tihmstar on 14.09.16.
//  Copyright © 2016 tihmstar. All rights reserved.
//

#include <getopt.h>
#include "futurerestore.hpp"

extern "C"{
#include "tsschecker.h"
extern void restore_set_ignore_bb_fail(int input);
};

#include <libgeneral/macros.h>
#include <img4tool/img4tool.hpp>
#ifdef HAVE_LIBIPATCHER
#include <libipatcher/libipatcher.hpp>
#endif

#ifdef WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

static struct option longopts[] = {
        { "apticket",                   required_argument,      nullptr, 't' },
        { "baseband",                   required_argument,      nullptr, 'b' },
        { "baseband-manifest",          required_argument,      nullptr, 'p' },
        { "sep",                        required_argument,      nullptr, 's' },
        { "sep-manifest",               required_argument,      nullptr, 'm' },
        { "wait",                       no_argument,            nullptr, 'w' },
        { "update",                     no_argument,            nullptr, 'u' },
        { "debug",                      no_argument,            nullptr, 'd' },
        { "exit-recovery",              no_argument,            nullptr, 'e' },
        { "custom-latest",              required_argument,      nullptr, 'c' },
        { "custom-latest-buildid",      required_argument,      nullptr, 'g' },
        { "help",                       no_argument,            nullptr, 'h' },
        { "custom-latest-beta",         no_argument,            nullptr, 'i' },
        { "custom-latest-ota",          no_argument,            nullptr, 'k' },
        { "latest-sep",                 no_argument,            nullptr, '0' },
        { "no-restore",                 no_argument,            nullptr, 'z' },
        { "latest-baseband",            no_argument,            nullptr, '1' },
        { "no-baseband",                no_argument,            nullptr, '2' },
        { "no-rsep",                    no_argument,            nullptr, 'j' },
#ifdef HAVE_LIBIPATCHER
        { "use-pwndfu",                 no_argument,            nullptr, '3' },
        { "no-ibss",                    no_argument,            nullptr, '4' },
        { "rdsk",                       required_argument,      nullptr, '5' },
        { "rkrn",                       required_argument,      nullptr, '6' },
        { "set-nonce",                  optional_argument,      nullptr, '7' },
        { "serial",                     no_argument,            nullptr, '8' },
        { "boot-args",                  required_argument,      nullptr, '9' },
        { "no-cache",                   no_argument,            nullptr, 'a' },
        { "skip-blob",                  no_argument,            nullptr, 'f' },
#endif
        { nullptr, 0, nullptr, 0 }
};

#define FLAG_WAIT                   1 << 0
#define FLAG_UPDATE                 1 << 1
#define FLAG_LATEST_SEP             1 << 2
#define FLAG_LATEST_BASEBAND        1 << 3
#define FLAG_NO_BASEBAND            1 << 4
#define FLAG_IS_PWN_DFU             1 << 5
#define FLAG_NO_IBSS                1 << 6
#define FLAG_RESTORE_RAMDISK        1 << 7
#define FLAG_RESTORE_KERNEL         1 << 8
#define FLAG_SET_NONCE              1 << 9
#define FLAG_SERIAL                 1 << 10
#define FLAG_BOOT_ARGS              1 << 11
#define FLAG_NO_CACHE               1 << 12
#define FLAG_SKIP_BLOB              1 << 13
#define FLAG_NO_RESTORE_FR          1 << 14
#define FLAG_CUSTOM_LATEST          1 << 15
#define FLAG_CUSTOM_LATEST_BUILDID  1 << 16
#define FLAG_CUSTOM_LATEST_BETA     1 << 17
#define FLAG_CUSTOM_LATEST_OTA      1 << 18
#define FLAG_NO_RSEP_FR             1 << 19
#define FLAG_IGNORE_BB_FAIL         1 << 20

bool manual = false;

void cmd_help(){
    printf("使用方法: futurerestore [选项] IPSW\n");
    printf("允许使用自定义SEP+基带恢复到不匹配的固件\n");
    printf("\n常规选项:\n");
    printf("  -h, --help\t\t\t\t显示使用帮助\n");
    printf("  -t, --apticket PATH\t\t\t用于恢复的签名票证\n");
    printf("  -u, --update\t\t\t\t更新而不是抹除安装 (需要适当的APTicket)\n");
    printf("              \t\t\t\t如果您从已越狱的固件更新，请不要使用此参数！\n");
    printf("  -w, --wait\t\t\t\t继续重启，直到ApNonce匹配APTicket (ApNonce碰撞，不可靠)\n");
    printf("  -d, --debug\t\t\t\t显示所有代码，用于保存调试测试日志\n");
    printf("  -e, --exit-recovery\t\t\t退出恢复模式并退出\n");
    printf("  -z, --no-restore\t\t\t在发送NOR数据之前不恢复和结束\n");
    printf("  -c, --custom-latest VERSION\t\t指定用于SEP、基带和其他FirmwareUpdater组件的自定义最新版本\n");
    printf("  -g, --custom-latest-buildid BUILDID\t指定用于SEP、基带和其他FirmwareUpdater组件的自定义最新Build ID\n");
    printf("  -i, --custom-latest-beta\t\t从测试版固件列表中获取自定义链接\n");
    printf("  -k, --custom-latest-ota\t\t从OTA固件列表中获取自定义链接");

#ifdef HAVE_LIBIPATCHER
    printf("\nOdysseus降级选项:\n");
    printf("  -3, --use-pwndfu\t\t\t使用Odysseus方法恢复设备，设备需要已经处于pwed DFU模式\n");
    printf("  -4, --no-ibss\t\t\t\t使用Odysseus方法恢复设备，特别是对于checkm8/iPwnder32，除非iPwnder已经为引导程序打好的补丁\n");
    printf("  -5, --rdsk PATH\t\t\t设置自定义还原ramdisk以进入还原模式 (需要 use-pwndfu)\n");
    printf("  -6, --rkrn PATH\t\t\t设置自定义恢复内核缓存以进入还原模式(需要 use-pwndfu)\n");
    printf("  -7, --set-nonce\t\t\t从Blob设置自定义Nonce，然后退出恢复(需要 use-pwndfu)\n");
    printf("  -7, --set-nonce=0xNONCE\t\t设置自定义Nonce，然后退出恢复(需要 use-pwndfu)\n");
    printf("  -8, --serial\t\t\t\t在引导期间启用串行 (需要串行电缆和use-pwndfu)\n");
    printf("  -9, --boot-args\t\t\t设置自定义还原启动参数 (谨慎使用，且需要 use-pwndfu)\n");
    printf("  -a, --no-cache\t\t\t禁用已缓存并已修补的iBSS/iBEC (需要 use-pwndfu)\n");
    printf("  -f, --skip-blob\t\t\t跳过SHSH Blob验证 (谨慎使用，且需要 use-pwndfu)\n");
#endif

    printf("\nSEP选项:\n");
    printf("  -0, --latest-sep\t\t\t使用最新签名的SEP而不是手动指定一个\n");
    if(manual) {
        printf("  -s, --sep PATH\t\t\tSEP刷新\n");
        printf("  -m, --sep-manifest PATH\t\t用于请求SEP Ticket的BuildManifest\n");
    }
    printf("  -j, --no-rsep\t\t\t\t选择不发送还原模式SEP固件命令\n");

    printf("\n基带选项:\n");
    printf("  -1, --latest-baseband\t\t\t使用最新签名的基带而不是手动指定一个\n");
    if(manual) {
        printf("  -b, --baseband PATH\t\t\t基带刷新\n");
        printf("  -p, --baseband-manifest PATH\t\t用于请求基带Ticket的BuildManifest\n");
    }
    printf("  -2, --no-baseband\t\t\t跳过检查，不刷新基带\n");
    printf("                   \t\t\t仅适用于没有基带的设备 (例如iPod Touch或仅支持WiFi的iPad)\n\n");
}

using namespace std;
using namespace tihmstar;
int main_r(int argc, const char * argv[]) {
#ifdef WIN32
    DWORD termFlags;
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleMode(handle, &termFlags))
        SetConsoleMode(handle, termFlags | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
    int err=0;
    printf("版本: " VERSION_RELEASE "(" VERSION_COMMIT_SHA "-" VERSION_COMMIT_COUNT ")\n");
    printf("%s\n",tihmstar::img4tool::version());
#ifdef HAVE_LIBIPATCHER
    printf("%s\n",libipatcher::version());
    printf("Odysseus支持32位: yes\n");
    printf("Odysseus支持64位: %s\n",(libipatcher::has64bitSupport() ? "yes" : "no"));
#else
    printf("Odysseus支持: no\n");
#endif

    int optindex = 0;
    int opt;
    long flags = 0;
    bool exitRecovery = false;

    const char *ipsw = nullptr;
    const char *basebandPath = nullptr;
    const char *basebandManifestPath = nullptr;
    const char *sepPath = nullptr;
    const char *sepManifestPath = nullptr;
    const char *bootargs = nullptr;
    std::string customLatest;
    std::string customLatestBuildID;
    const char *ramdiskPath = nullptr;
    const char *kernelPath = nullptr;
    const char *custom_nonce = nullptr;

    vector<const char*> apticketPaths;

    t_devicevals devVals = {nullptr};
    t_iosVersion versVals = {nullptr};

    char *legacy = std::getenv("FUTURERESTORE_I_SOLEMNLY_SWEAR_THAT_I_AM_UP_TO_NO_GOOD");
    manual = legacy != nullptr;
    if(manual) {
        info("警告：用户指定启用了已弃用/旧的选项，有无限重启的风险！\n");
    }

    if (argc == 1){
        cmd_help();
        return -1;
    }

    while ((opt = getopt_long(argc, (char* const *)argv, "ht:b:p:s:m:c:g:ikwude0z123456789afj", longopts, &optindex)) > 0) {
        switch (opt) {
            case 'h': // long option: "help"; can be called as short option
                cmd_help();
                return 0;
                break;
            case 't': // long option: "apticket"; can be called as short option
                apticketPaths.push_back(optarg);
                break;
            case 'b': // long option: "baseband"; can be called as short option
                retassure(manual, "--baseband 已弃用！请切换到 --custom-latest 或者 --custom-latest-beta");
                basebandPath = optarg;
                break;
            case 'p': // long option: "baseband-manifest"; can be called as short option
                retassure(manual, "--baseband-manifest 已弃用！请切换到 --custom-latest 或者 --custom-latest-beta.");
                basebandManifestPath = optarg;
                break;
            case 's': // long option: "sep"; can be called as short option
                retassure(manual, "--sep 已弃用！请切换到 --custom-latest 或者 --custom-latest-beta.");
                sepPath = optarg;
                break;
            case 'm': // long option: "sep-manifest"; can be called as short option
                retassure(manual, "--sep-manifest 已弃用！请切换到 --custom-latest 或者 --custom-latest-beta.");
                sepManifestPath = optarg;
                break;
            case 'w': // long option: "wait"; can be called as short option
                flags |= FLAG_WAIT;
                break;
            case 'u': // long option: "update"; can be called as short option
                flags |= FLAG_UPDATE;
                break;
            case 'c': // long option: "custom-latest"; can be called as short option
                customLatest = (optarg) ? std::string(optarg) : std::string("");
                flags |= FLAG_CUSTOM_LATEST;
                break;
            case 'g': // long option: "custom-latest-buildid"; can be called as short option
                customLatestBuildID = (optarg) ? std::string(optarg) : std::string("");
                flags |= FLAG_CUSTOM_LATEST_BUILDID;
                break;
            case 'i': // long option: "custom-latest-beta"; can be called as short option
                flags |= FLAG_CUSTOM_LATEST_BETA;
                break;
            case 'k': // long option: "custom-latest-ota"; can be called as short option
                flags |= FLAG_CUSTOM_LATEST_OTA;
                break;
            case '0': // long option: "latest-sep";
                flags |= FLAG_LATEST_SEP;
                break;
            case '1': // long option: "latest-baseband";
                flags |= FLAG_LATEST_BASEBAND;
                break;
            case '2': // long option: "no-baseband";
                flags |= FLAG_NO_BASEBAND;
                break;
            case 'j': // long option: "no-rsep";
                flags |= FLAG_NO_RSEP_FR;
                break;
#ifdef HAVE_LIBIPATCHER
            case '3': // long option: "use-pwndfu";
                flags |= FLAG_IS_PWN_DFU;
                break;
            case '4': // long option: "no-ibss";
                flags |= FLAG_NO_IBSS;
                break;
            case '5': // long option: "rdsk";
                flags |= FLAG_RESTORE_RAMDISK;
                ramdiskPath = optarg;
                break;
            case '6': // long option: "rkrn";
                flags |= FLAG_RESTORE_KERNEL;
                kernelPath = optarg;
                break;
            case '7': // long option: "set-nonce";
                flags |= FLAG_SET_NONCE;
                custom_nonce = (optarg) ? optarg : nullptr;
                if(custom_nonce != nullptr) {
                    uint64_t gen;
                    retassure(strlen(custom_nonce) == 16 || strlen(custom_nonce) == 18,"Nonce长度不正确！\n");
                    gen = std::stoul(custom_nonce, nullptr, 16);
                    retassure(gen, "解析生成器失败，确保格式为0x%16llx");
                }
                break;
            case '8': // long option: "serial";
                flags |= FLAG_SERIAL;
                break;
            case '9': // long option: "boot-args";
                flags |= FLAG_BOOT_ARGS;
                bootargs = (optarg) ? optarg : nullptr;
                break;
            case 'a': // long option: "no-cache";
                flags |= FLAG_NO_CACHE;
                break;
            case 'f': // long option: "skip-blob";
                flags |= FLAG_SKIP_BLOB;
                break;
#endif
            case 'e': // long option: "exit-recovery"; can be called as short option
                exitRecovery = true;
                break;
            case 'z': // long option: "no-restore"; can be called as short option
                flags |= FLAG_NO_RESTORE_FR;
                break;
            case 'd': // long option: "debug"; can be called as short option
                idevicerestore_debug = 1;
                break;
            default:
                cmd_help();
                return -1;
        }
    }

    if (argc-optind == 1) {
        argv += optind;

        ipsw = argv[0];
    }else if (argc == optind && flags & FLAG_WAIT) {
        info("用户请求只等待ApNonce匹配，而不是实际还原\n");
    }else if (exitRecovery){
        info("正在从恢复模式退出到正常模式\n");
    }else{
        error("参数解析失败！agrc=%d optind=%d\n",argc,optind);
        if (idevicerestore_debug){
            for (int i=0; i<argc; i++) {
                printf("argv[%d]=%s\n",i,argv[i]);
            }
        }
        return -5;
    }

    futurerestore client(flags & FLAG_UPDATE, flags & FLAG_IS_PWN_DFU, flags & FLAG_NO_IBSS, flags & FLAG_SET_NONCE, flags & FLAG_SERIAL, flags & FLAG_NO_RESTORE_FR, flags & FLAG_NO_RSEP_FR);
    retassure(client.init(),"无法初始化，找不到设备\n");

    printf("futurerestore初始化完成\n");
    if(flags & FLAG_NO_IBSS)
        retassure((flags & FLAG_IS_PWN_DFU),"--no-ibss 需要 --use-pwndfu\n");
    if(flags & FLAG_RESTORE_RAMDISK)
        retassure((flags & FLAG_IS_PWN_DFU),"--rdsk 需要 --use-pwndfu\n");
    if(flags & FLAG_RESTORE_KERNEL)
        retassure((flags & FLAG_IS_PWN_DFU),"--rkrn 需要 --use-pwndfu\n");
    if(flags & FLAG_SET_NONCE)
        retassure((flags & FLAG_IS_PWN_DFU),"--set-nonce 需要 --use-pwndfu\n");
    if(flags & FLAG_SET_NONCE && client.is32bit())
        error("--set-nonce 不支持32位设备\n");
    if(flags & FLAG_RESTORE_RAMDISK)
        retassure((flags & FLAG_RESTORE_KERNEL),"--rdsk 需要 --rkrn\n");
    if(flags & FLAG_SERIAL) {
        retassure((flags & FLAG_IS_PWN_DFU),"--serial 需要 --use-pwndfu\n");
        retassure(!(flags & FLAG_BOOT_ARGS),"--serial 与 --boot-args冲突\n");
    }
    if(flags & FLAG_BOOT_ARGS)
        retassure((flags & FLAG_IS_PWN_DFU),"--boot-args 需要 --use-pwndfu\n");
    if(flags & FLAG_NO_CACHE)
        retassure((flags & FLAG_IS_PWN_DFU),"--no-cache 需要 --use-pwndfu\n");
    if(flags & FLAG_SKIP_BLOB)
        retassure((flags & FLAG_IS_PWN_DFU),"--skip-blob 需要 --use-pwndfu\n");
    if(flags & FLAG_CUSTOM_LATEST_BETA)
        retassure((flags & FLAG_CUSTOM_LATEST_BUILDID),"-i, --custom-latest-beta 需要 -g, --custom-latest-buildid\n");
    if(flags & FLAG_CUSTOM_LATEST_OTA)
        retassure((flags & FLAG_CUSTOM_LATEST_BUILDID),"-k, --custom-latest-ota 需要 -g, --custom-latest-buildid\n");
    if(flags & FLAG_CUSTOM_LATEST_BUILDID)
        retassure((flags & FLAG_CUSTOM_LATEST) == 0,"-g, --custom-latest-buildid 与 -c, --custom-latest不兼容\n");

    if (exitRecovery) {
        client.exitRecovery();
        info("Done\n");
        return 0;
    }

    try {
        if (!apticketPaths.empty()) {
            client.loadAPTickets(apticketPaths);
        }

        if(!customLatest.empty()) {
            client.setCustomLatest(customLatest);
        }
        if(!customLatestBuildID.empty()) {
            client.setCustomLatestBuildID(customLatestBuildID, (flags & FLAG_CUSTOM_LATEST_BETA) != 0, (flags & FLAG_CUSTOM_LATEST_OTA) != 0);
        }
        if (!(
                ((!apticketPaths.empty() && ipsw)
                 && ((basebandPath && basebandManifestPath) || ((flags & FLAG_LATEST_BASEBAND) || (flags & FLAG_NO_BASEBAND)))
                 && ((sepPath && sepManifestPath) || (flags & FLAG_LATEST_SEP) || client.is32bit())
                ) || (ipsw && (flags & FLAG_IS_PWN_DFU))
        )) {

            if (!(flags & FLAG_WAIT) || ipsw){
                error("缺少一些参数！\n");
                cmd_help();
                err = -2;
            }else{
                client.putDeviceIntoRecovery();
                client.waitForNonce();
                info("Done\n");
            }
            goto error;
        }

        devVals.deviceModel = (char*)client.getDeviceModelNoCopy();
        devVals.deviceBoard = (char*)client.getDeviceBoardNoCopy();

        if(flags & FLAG_RESTORE_RAMDISK) {
            client.setRamdiskPath(ramdiskPath);
            client.loadRamdisk(ramdiskPath);
        }

        if(flags & FLAG_RESTORE_KERNEL) {
            client.setKernelPath(kernelPath);
            client.loadKernel(kernelPath);
        }

        if(flags & FLAG_SET_NONCE) {
            client.setNonce(custom_nonce);
        }

        if(flags & FLAG_BOOT_ARGS) {
            client.setBootArgs(bootargs);
        }

        if(flags & FLAG_NO_CACHE) {
            client.disableCache();
        }

        if(flags & FLAG_SKIP_BLOB) {
            client.skipBlobValidation();
        }
        if(!(flags & FLAG_SET_NONCE)) {
            if (flags & FLAG_LATEST_SEP) {
                info("用户指定使用最新签名的SEP\n");
                client.downloadLatestSep();
            } else if (!client.is32bit()) {
                client.setSepPath(sepPath);
                client.setSepManifestPath(sepManifestPath);
                client.loadSep(sepPath);
                client.loadSepManifest(sepManifestPath);
            }
        }
        if(flags & FLAG_IGNORE_BB_FAIL) {
            restore_set_ignore_bb_fail(1);
        }

        versVals.basebandMode = kBasebandModeWithoutBaseband;
        if(!(flags & FLAG_SET_NONCE)) {
            info("正在检查SEP是否处于签名状态...\n");
            if (!client.is32bit() &&
                !(isManifestSignedForDevice(client.getSepManifestPath().c_str(), &devVals, &versVals, nullptr))) {
                reterror("SEP固件未处于签名状态！\n");
            } else {
                info("SEP处于签名状态！\n");
            }
        }
        if (flags & FLAG_NO_BASEBAND){
            info("\n警告：用户指定了是不刷新基带。如果设备需要基带，这可能会使还原失败！\n");
            info("\n如果您错误地添加了此选项，现在可以按CTRL+C取消\n");
            int c = 10;
            info("正在继续还原 ");
            while (c) {
                info("%d ",c--);
                fflush(stdout);
                sleep(1);
            }
            info("");
        }else{
            if(!(flags & FLAG_SET_NONCE)) {
                if (flags & FLAG_LATEST_BASEBAND) {
                    info("用户指定使用最新签名的基带\n");
                    client.downloadLatestBaseband();
                } else {
                    client.setBasebandPath(basebandPath);
                    client.setBasebandManifestPath(basebandManifestPath);
                    client.loadBaseband(basebandPath);
                    client.loadBasebandManifest(basebandManifestPath);
                    info("已设置了SEP和基带路径和固件\n");
                }
            }

            versVals.basebandMode = kBasebandModeOnlyBaseband;
            if (!(devVals.bbgcid = client.getBasebandGoldCertIDFromDevice())){
                debug("[警告] 正在使用Tsschecker的回退来获取Baseband GoldCertID，这可能会导致基带签名状态信息无效！\n");
            }
            if(!(flags & FLAG_SET_NONCE)) {
                info("正在检查基带是否处于签名状态...\n");
                if (!(isManifestSignedForDevice(client.getBasebandManifestPath().c_str(), &devVals, &versVals,
                                                nullptr))) {
                    reterror("基带固件未处于签名状态！\n");
                } else {
                    info("基带处于签名状态！\n");
                }
            }
        }
        if(!client.is32bit() && !(flags & FLAG_SET_NONCE)) {
            client.downloadLatestFirmwareComponents();
        }
        client.putDeviceIntoRecovery();
        if (flags & FLAG_WAIT){
            client.waitForNonce();
        }
    } catch (int error) {
        err = error;
        printf("[错误] 失败代码=%d\n",err);
        goto error;
    }

    try {
        client.doRestore(ipsw);
        printf("执行完成: 还原成功！\n");
    } catch (tihmstar::exception &e) {
        e.dump();
        printf("执行完成: 还原失败！\n");
    }

    error:
    if (err){
        printf("失败，错误代码为=%d\n",err);
    }
    return err;
#undef reterror
}

int main(int argc, const char * argv[]) {
#ifdef DEBUG
    return main_r(argc, argv);
#else
    try {
        return main_r(argc, argv);
    } catch (tihmstar::exception &e) {
        printf("%s: 失败，出现异常:\n",PACKAGE_NAME);
        e.dump();
        return e.code();
    }
#endif
}
