#pragma once

#include <DbgHelp.h>
#pragma comment(lib, "DbgHelp.lib")
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")

namespace Utils {
    namespace CallstackCapture {
        // 地址信息结构
        struct AddressInfo {
            uintptr_t runtimeAddress;
            uintptr_t staticAddress;  // RVA (相对虚拟地址)
            std::string moduleName;
            uintptr_t moduleBase;
        };
        
        // 调用栈帧结构
        struct CallStackFrame {
            uintptr_t returnAddress;
            std::vector<uintptr_t> stackFrames;
            std::vector<AddressInfo> addressInfos; // 对应stackFrames的地址信息
            int callCount;
            std::string firstSeenCallstack;
        };

        // 调用地址记录类型
        using CallSitesMap = std::unordered_map<uintptr_t, CallStackFrame>;

        // 获取地址对应的模块信息和静态地址
        AddressInfo GetAddressInfo(uintptr_t address);
        
        // 格式化地址信息为字符串
        std::string FormatAddressInfo(const AddressInfo& info);

        // 捕获完整调用栈信息，返回字符串格式
        std::string CaptureCallstack(int skipFrames = 1, int maxFrames = 32);

        // 获取调用栈地址列表
        std::vector<uintptr_t> GetCallstack(int skipFrames = 1, int maxFrames = 16);

        // 记录新的调用地址
        void RecordCallSite(CallSitesMap& callSites, uintptr_t returnAddr, const char* callType);

        // 导出调用地址统计信息到日志和文件
        void DumpCallSites(const CallSitesMap& callSites, const char* filename);

        // 分析调用地址模式并输出到文件
        void AnalyzeCallSites(const CallSitesMap& callSites, const char* filename);

        // 清除调用地址数据
        void ClearCallSites(CallSitesMap& callSites);
    }
} 