#include <getopt.h>
#include <erofs/erofs_io.h>
#include <erofs/erofs_compress.h>
#include <erofs_compressor.h>
#include <erofs/erofs_config.h>
#include <erofs/erofs_print.h>
#include <sys/time.h>

#include "ExtractState.h"
#include "ExtractOperation.h"
#include "Logging.h"

#if defined(__CYGWIN__) || defined(_WIN32)
#include "CaseSensitiveInfo.h"
#endif

#include <windows.h>

using namespace skkk;

static inline void get_available_compressors(string &ret)
{
    int i = 0;
    bool comma = false;
    const struct erofs_algorithm *s;

    while ((s = z_erofs_list_available_compressors(&i)) != nullptr) {
        if (comma)
            ret.append(", ");
        ret.append(s->name);
        comma = true;
    }
}

static inline void usage() {
    char buf[1536] = {0};
    snprintf(buf, 1536,
             "usage: [options]\n"
             "-h, --help              Display this help and exit\n"
             "-i, --image=[FILE]      Image file\n"
             "--offset=#              skip # bytes at the beginning of IMAGE\n"
             "-p                      Print all entrys\n"
             "-P, --print=X           Print the target of path X\n"
             "-x                      Extract all items\n"
             "-X, --extract=X         Extract the target of path X\n"
             "-c, --config=[FILE]     Target of config\n"
             "-r                      When using config, recurse directories\n"
             "-s                      Silent mode, Don't show progress\n"
             "-f, --overwrite         [default: skip] overwrite files that already exist\n"
             "-T#                     [1-%u] Use # threads, -T0: %u\n"
             "--only-cfg              Only extract fs_config|file_contexts|fs_options\n"
             "-o, --outdir=X          Output dir\n"
             "-V, --version           Print the version info\n",
             eo->limitHardwareConcurrency,
             eo->hardwareConcurrency
    );
    fputs(buf, stderr);
}

static inline void print_version() {
    string compressors;
    get_available_compressors(compressors);
    printf("  " BROWN "erofs-utils:" COLOR_NONE "            " RED2_BOLD "%s" COLOR_NONE "\n", cfg.c_version);
    printf("  " BROWN "extract.erofs:" COLOR_NONE "          " RED2_BOLD "1.0.6" COLOR_NONE "\n");
    printf("  " BROWN "Available compressors:" COLOR_NONE "  " RED2_BOLD "%s" COLOR_NONE "\n", compressors.c_str());
    printf("  " BROWN "extract author:" COLOR_NONE "         " RED2_BOLD "skkk" COLOR_NONE "\n");
}

static struct option arg_options[] = {
    {"help",      no_argument,       nullptr, 'h'},
    {"version",   no_argument,       nullptr, 'V'},
    {"image",     required_argument, nullptr, 'i'},
    {"offset",    required_argument, nullptr, 2},
    {"outdir",    required_argument, nullptr, 'o'},
    {"print",     required_argument, nullptr, 'P'},
    {"overwrite", no_argument,       nullptr, 'f'},
    {"extract",   required_argument, nullptr, 'X'},
    {"config",    required_argument, nullptr, 'c'},
    {"only-cfg",  no_argument,       nullptr, 1},
    {nullptr,     no_argument,       nullptr, 0},
};

static int parseAndCheckExtractCfg(int argc, char **argv) {
    int opt;
    int rc = RET_EXTRACT_CONFIG_FAIL;
    bool enterParseOpt = false;
    while ((opt = getopt_long(argc, argv, "hi:psxfrc:P:T:o:X:V", arg_options, nullptr)) != -1) {
        enterParseOpt = true;
        switch (opt) {
            case 'h':
                usage();
                goto exit;
            case 'V':
                print_version();
                goto exit;
            case 'i':
                if (optarg) {
                    eo->setImgPath(optarg);
                }
                LOGCD("imgPath=%s", eo->getImgPath().c_str());
                break;
            case 'o':
                if (optarg) {
                    eo->setOutDir(optarg);
                }
                LOGCD("outDir=%s", eo->getOutDir().c_str());
                break;
            case 'p':
                eo->isPrintAllNode = true;
                LOGCD("isPrintAllNode=%d", eo->isPrintAllNode);
                break;
            case 'P':
                eo->isPrintTarget = true;
                if (optarg) eo->targetPath = optarg;
                LOGCD("isPrintTarget=%d targetPath=%s", eo->isPrintTarget, eo->targetPath.c_str());
                break;
            case 'f':
                eo->overwrite = true;
                LOGCD("overwrite=%d", eo->overwrite);
                break;
            case 'x':
                eo->check_decomp = true;
                eo->isExtractAllNode = true;
                LOGCD("isExtractAllNode=%d check_decomp=%d", eo->isExtractAllNode, eo->check_decomp);
                break;
            case 'X':
                eo->check_decomp = true;
                eo->isExtractTarget = true;
                if (optarg) eo->targetPath = optarg;
                LOGCD("isExtractTarget=%d targetPath=%s", eo->isExtractTarget, eo->targetPath.c_str());
                break;
            case 'c':
                eo->isExtractTargetConfig = true;
                if (optarg) eo->targetConfigPath = optarg;
                LOGCD("targetConfig=%s", eo->targetConfigPath.c_str());
                break;
            case 's':
                eo->isSilent = true;
                LOGCD("isSilent=%d", eo->isSilent);
                break;
            case 'r':
                eo->targetConfigRecurse = true;
                LOGCD("targetConfigRecurse=%d", eo->targetConfigRecurse);
                break;
            case 'T':
                if (optarg) {
                    char *endPtr;
                    uint64_t n = strtoull(optarg, &endPtr, 0);
                    if (*endPtr == '\0') {
                        eo->useMultiThread = true;
                        eo->threadNum = n;
                    }
                }
                break;
            case 1:
                eo->extractOnlyConfAndSeLabel = true;
                LOGCD("extractOnlyConfAndSeLabel=%d", eo->extractOnlyConfAndSeLabel);
                break;
            case 2:
                if (optarg) {
                    char *endPtr;
                    uint64_t n = strtoull(optarg, &endPtr, 0);
                    if (*endPtr == '\0') {
                        g_sbi.bdev.offset = n;
                        LOGCD("offset=%lu", g_sbi.bdev.offset);
                    }
                }
                break;
            default:
                usage();
                print_version();
                goto exit;
        }
    }

    if (enterParseOpt) {
        bool err;
        // check needed arg
        err = !eo->getImgPath().empty() && fileExists(eo->getImgPath());
        if (!err) {
            LOGCE("img file '%s' does not exist", eo->getImgPath().c_str());
            goto exit;
        }
        rc = !eo->initOutDir();
        if (!rc) {
            goto exit;
        }
        LOGCD("outDir=%s confDir=%s", eo->getOutDir().c_str(), eo->getConfDir().c_str());

        if (eo->useMultiThread) {
            if (eo->threadNum > eo->limitHardwareConcurrency) {
                rc = RET_EXTRACT_THREAD_NUM_ERROR;
                LOGCE("Threads min: 1 , max: %u", eo->limitHardwareConcurrency);
                goto exit;
            } else if (eo->threadNum == 0) {
                eo->threadNum = eo->hardwareConcurrency;
            }
            LOGCD("Threads num=%u", eo->threadNum);
        }
        rc = RET_EXTRACT_CONFIG_DONE;
    } else {
        usage();
    }

exit:
    return rc;
}

static inline void printOperationTime(struct timeval *start, struct timeval *end) {
    LOGCI(GREEN2_BOLD "The operation took: " COLOR_NONE RED2 "%.3f" COLOR_NONE "%s",
          (end->tv_sec - start->tv_sec) + static_cast<float>(end->tv_usec - start->tv_usec) / 1000000,
          GREEN2_BOLD " second(s)." COLOR_NONE
    );
}

int main(int argc, char **argv) {
    int ret = RET_EXTRACT_DONE, err;

    struct timeval start = {}, end = {};
    // Start time
    gettimeofday(&start, nullptr);

    // Initialize erofs config
    erofs_init_configure();
    cfg.c_dbg_lvl = EROFS_ERR;

    // Initialize extract config
    err = parseAndCheckExtractCfg(argc, argv);
    if (err != RET_EXTRACT_CONFIG_DONE) {
        ret = err;
        goto exit;
    }

    err = erofs_dev_open(&g_sbi, eo->getImgPath().c_str(), O_RDONLY);
    if (err) {
        ret = RET_EXTRACT_INIT_FAIL;
        LOGCE("failed to open '%s'", eo->getImgPath().c_str());
        goto exit;
    }

    err = erofs_read_superblock(&g_sbi);
    if (err) {
        ret = RET_EXTRACT_INIT_FAIL;
        LOGCE("failed to read superblock");
        goto exit_dev_close;
    }

    if (eo->isPrintTarget || eo->isExtractTarget || eo->isExtractTargetConfig)
        err = eo->initErofsNodeByTarget();
    else if (eo->isPrintAllNode || eo->isExtractAllNode)
        err = eo->initAllErofsNode();
    if (err) {
        ret = RET_EXTRACT_INIT_NODE_FAIL;
        goto exit_dev_close;
    }

    if (eo->isPrintTarget || eo->isPrintAllNode) {
        ExtractOperation::printInitializedNode();
        goto exit_dev_close;
    }

    LOGCI(GREEN2_BOLD "Starting..." COLOR_NONE);

    if ((eo->isExtractTarget || eo->isExtractAllNode) && eo->extractOnlyConfAndSeLabel) {
        err = eo->createExtractConfigDir();
        if (err) {
            ret = RET_EXTRACT_CREATE_DIR_FAIL;
            goto exit_dev_close;
        }
        eo->extractFsConfigAndSelinuxLabelAndFsOptions();
        goto end;
    }

    if (eo->isExtractTarget || eo->isExtractAllNode) {
        err = eo->createExtractConfigDir() & eo->createExtractOutDir();
        if (err) {
            ret = RET_EXTRACT_CREATE_DIR_FAIL;
            goto exit_dev_close;
        }
#if defined(__CYGWIN__) || defined(_WIN32)
        // Dir must exist and empty.
        //if (EnsureCaseSensitive(eo->getOutDir().c_str()) == 0)
        //    LOGCI("Success change case sensitive.");
        //else
        //    LOGCW("Failed change case sensitive.");
#endif
        eo->extractFsConfigAndSelinuxLabelAndFsOptions();
        eo->useMultiThread ? eo->extractErofsNodeMultiThread(eo->isSilent) : eo->extractErofsNode(eo->isSilent);
        goto end;
    }

end:
    // End time
    gettimeofday(&end, nullptr);
    printOperationTime(&start, &end);

exit_dev_close:
    erofs_dev_close(&g_sbi);
    LOGCD("ErofsNode size=%lu", eo->getErofsNodes().size());
    LOGCD("main exit ret=%d", ret);

exit:
    erofs_blob_closeall(&g_sbi);
    erofs_exit_configure();
    ExtractOperation::erofsOperationExit();
    return ret;
}
