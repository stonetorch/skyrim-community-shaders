#include "CallstackCapture.h"
#include <Windows.h>

namespace Utils {
    namespace CallstackCapture {
        // 获取地址对应的模块信息和静态地址
        AddressInfo GetAddressInfo(uintptr_t address) {
            AddressInfo info = {};
            info.runtimeAddress = address;

            HANDLE process = GetCurrentProcess();
            HMODULE hModule;
            DWORD cbNeeded;

            // 只获取第一个模块，即主模块
            if (EnumProcessModules(process, &hModule, sizeof(hModule), &cbNeeded)) {
                MODULEINFO moduleInfo;
                if (GetModuleInformation(process, hModule, &moduleInfo, sizeof(moduleInfo))) {
                    // 获取模块的地址范围
                    uintptr_t moduleBase = reinterpret_cast<uintptr_t>(moduleInfo.lpBaseOfDll);
                    uintptr_t moduleEnd = moduleBase + moduleInfo.SizeOfImage;

                    // 检查地址是否在主模块范围内
                    if (address >= moduleBase && address < moduleEnd) {
                        info.moduleBase = moduleBase;
                        info.staticAddress = address - moduleBase;

                        // 获取模块名称
                        char modulePath[MAX_PATH];
                        if (GetModuleFileNameA(hModule, modulePath, MAX_PATH)) {
                            std::string fullPath(modulePath);
                            size_t lastSlash = fullPath.find_last_of("\\/");
                            info.moduleName = (lastSlash != std::string::npos)
                                                  ? fullPath.substr(lastSlash + 1)
                                                  : fullPath;
                        } else {
                            info.moduleName = "主模块";
                        }
                        return info;
                    }
                }
            }

            // 如果不在主模块中，检查其他已加载的模块
            HMODULE modules[1024];
            if (EnumProcessModules(process, modules, sizeof(modules), &cbNeeded)) {
                DWORD moduleCount = cbNeeded / sizeof(HMODULE);

                for (DWORD i = 0; i < moduleCount; i++) {
                    MODULEINFO moduleInfo;
                    if (GetModuleInformation(process, modules[i], &moduleInfo, sizeof(moduleInfo))) {
                        uintptr_t moduleBase = reinterpret_cast<uintptr_t>(moduleInfo.lpBaseOfDll);
                        uintptr_t moduleEnd = moduleBase + moduleInfo.SizeOfImage;

                        if (address >= moduleBase && address < moduleEnd) {
                            info.moduleBase = moduleBase;
                            info.staticAddress = address - moduleBase;

                            // 获取模块名称
                            char modulePath[MAX_PATH];
                            if (GetModuleFileNameA(modules[i], modulePath, MAX_PATH)) {
                                std::string fullPath(modulePath);
                                size_t lastSlash = fullPath.find_last_of("\\/");
                                info.moduleName = (lastSlash != std::string::npos)
                                                      ? fullPath.substr(lastSlash + 1)
                                                      : fullPath;
                            } else {
                                info.moduleName = "未知模块";
                            }
                            return info;
                        }
                    }
                }
            }

            // 如果找不到对应模块
            info.moduleBase = 0;
            info.staticAddress = address;
            info.moduleName = "未知";
            return info;
        }

        // 格式化地址信息为字符串
        std::string FormatAddressInfo(const AddressInfo &info) {
            std::stringstream ss;
            ss << info.moduleName << "+0x" << std::hex << std::setw(8) << std::setfill('0') << info.staticAddress
                    << " (运行时: 0x" << std::setw(16) << info.runtimeAddress << ")";
            return ss.str();
        }

        // 捕获完整调用栈信息，返回字符串格式
        std::string CaptureCallstack(int skipFrames, int maxFrames) {
            void *stack[32];
            HANDLE process = GetCurrentProcess();
            SymInitialize(process, NULL, TRUE);
            WORD frames = CaptureStackBackTrace(skipFrames, maxFrames, stack, NULL);

            std::stringstream callstack;
            callstack << "Callstack (" << frames << " frames):\n";

            // 分配符号信息的内存
            SYMBOL_INFO *symbol = (SYMBOL_INFO *) calloc(sizeof(SYMBOL_INFO) + 256, 1);
            if (!symbol) {
                callstack << "无法分配符号信息内存\n";
                return callstack.str();
            }

            symbol->MaxNameLen = 255;
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

            for (int i = 0; i < frames; i++) {
                uintptr_t address = reinterpret_cast<uintptr_t>(stack[i]);
                AddressInfo addressInfo = GetAddressInfo(address);

                BOOL hr = SymFromAddr(process, (DWORD64) (stack[i]), 0, symbol);
                DWORD displacement;
                IMAGEHLP_LINE64 line;
                line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

                if (hr && SymGetLineFromAddr64(process, (DWORD64) stack[i], &displacement, &line)) {
                    callstack << i << ": " << symbol->Name << " [" << FormatAddressInfo(addressInfo) << "]"
                            << " at " << line.FileName << ":" << std::dec << line.LineNumber << "\n";
                } else if (hr) {
                    callstack << i << ": " << symbol->Name << " [" << FormatAddressInfo(addressInfo) << "]\n";
                } else {
                    callstack << i << ": [" << FormatAddressInfo(addressInfo) << "] - 无符号信息\n";
                }
            }

            free(symbol);
            return callstack.str();
        }

        // 获取调用栈地址列表
        std::vector<uintptr_t> GetCallstack(int skipFrames, int maxFrames) {
            std::vector<uintptr_t> result;
            void *stack[16];
            WORD frames = CaptureStackBackTrace(skipFrames, maxFrames, stack, NULL);

            for (int i = 0; i < frames; i++) {
                result.push_back(reinterpret_cast<uintptr_t>(stack[i]));
            }

            return result;
        }

        // 记录新的调用地址
        void RecordCallSite(CallSitesMap &callSites, uintptr_t returnAddr, const char *callType) {
            auto it = callSites.find(returnAddr);
            if (it == callSites.end()) {
                std::string callstack = CaptureCallstack(3); // 跳过当前函数、钩子函数和记录函数
                std::vector<uintptr_t> stackFrames = GetCallstack(3);

                // 获取每个地址的详细信息
                std::vector<AddressInfo> addressInfos;
                for (uintptr_t addr: stackFrames) {
                    addressInfos.push_back(GetAddressInfo(addr));
                }

                CallStackFrame frame;
                frame.returnAddress = returnAddr;
                frame.stackFrames = stackFrames;
                frame.addressInfos = addressInfos;
                frame.callCount = 1;
                frame.firstSeenCallstack = callstack;

                // 输出时包含静态地址信息
                AddressInfo returnAddrInfo = GetAddressInfo(returnAddr);
                // 暂时用printf替代logger，避免依赖问题
                printf("发现新的%s调用地址: %s\n%s", callType, FormatAddressInfo(returnAddrInfo).c_str(), callstack.c_str());
                callSites[returnAddr] = frame;
            } else {
                it->second.callCount++;
            }
        }

        // 导出调用地址统计信息到日志和文件
        void DumpCallSites(const CallSitesMap &callSites, const char *filename) {
            printf("导出DrawCall调用地址统计信息...\n");

            // 先输出到控制台
            std::stringstream ss;
            ss << "DrawCall调用地址统计：\n";
            ss << "静态地址\t\t运行时地址\t\t调用次数\n";

            for (const auto &[addr, frame]: callSites) {
                AddressInfo info = GetAddressInfo(addr);
                ss << info.moduleName << "+0x" << std::hex << std::setw(8) << std::setfill('0') << info.staticAddress
                        << "\t0x" << std::setw(16) << addr << "\t" << std::dec << frame.callCount << "\n";
            }

            printf("%s", ss.str().c_str());

            // 同时保存到文件
            try {
                std::ofstream outFile(filename);
                if (outFile.is_open()) {
                    outFile << "DrawCall调用地址统计报告\n";
                    outFile << "========================\n\n";

                    for (const auto &[addr, frame]: callSites) {
                        AddressInfo returnAddrInfo = GetAddressInfo(addr);
                        outFile << "返回地址: " << FormatAddressInfo(returnAddrInfo)
                                << " 调用次数: " << std::dec << frame.callCount << "\n";
                        outFile << "首次捕获调用栈:\n" << frame.firstSeenCallstack << "\n";

                        outFile << "调用栈帧地址详情:\n";
                        for (size_t i = 0; i < frame.stackFrames.size() && i < frame.addressInfos.size(); i++) {
                            outFile << i << ": " << FormatAddressInfo(frame.addressInfos[i]) << "\n";
                        }
                        outFile << "\n------------------------\n\n";
                    }

                    outFile.close();
                    printf("已将调用地址数据保存到 %s\n", filename);
                }
            } catch (const std::exception &e) {
                printf("保存调用地址数据到文件时出错: %s\n", e.what());
            }
        }

        // 分析调用地址模式并输出到文件
        void AnalyzeCallSites(const CallSitesMap &callSites, const char *filename) {
            // 首先确保我们有数据
            if (callSites.empty()) {
                printf("没有捕获到任何DrawCall调用地址\n");
                return;
            }

            printf("开始分析DrawCall调用地址...\n");

            // 按调用次数排序
            std::vector<std::pair<uintptr_t, const CallStackFrame *> > sortedSites;
            for (const auto &[addr, frame]: callSites) {
                sortedSites.push_back({addr, &frame});
            }

            std::sort(sortedSites.begin(), sortedSites.end(),
                      [](const auto &a, const auto &b) {
                          return a.second->callCount > b.second->callCount;
                      });

            // 分析调用堆栈模式（使用静态地址）
            std::unordered_map<std::string, std::vector<uintptr_t> > callPatterns;

            for (const auto &[addr, frame]: callSites) {
                // 创建一个基于静态地址的调用栈指纹
                std::stringstream pattern;
                for (size_t i = 0; i < std::min(size_t(3), frame.addressInfos.size()); i++) {
                    pattern << frame.addressInfos[i].moduleName << "+0x"
                            << std::hex << (frame.addressInfos[i].staticAddress & 0xFFFF) << "_";
                }
                std::string patternStr = pattern.str();

                callPatterns[patternStr].push_back(addr);
            }

            // 分析结果输出到文件
            try {
                std::ofstream outFile(filename);
                if (outFile.is_open()) {
                    outFile << "DrawCall调用地址分析报告\n";
                    outFile << "========================\n\n";

                    // 输出按频率排序的调用地址
                    outFile << "按调用频率排序的前10个调用地址:\n";
                    outFile << "----------------------------\n";
                    int count = 0;
                    for (const auto &[addr, frame]: sortedSites) {
                        if (count++ >= 10) break;
                        AddressInfo info = GetAddressInfo(addr);
                        outFile << count << ". " << FormatAddressInfo(info)
                                << " 调用次数: " << std::dec << frame->callCount << "\n";
                    }
                    outFile << "\n";

                    // 输出相似调用模式分析
                    outFile << "相似调用栈模式分析:\n";
                    outFile << "----------------------------\n";
                    for (const auto &[pattern, addresses]: callPatterns) {
                        if (addresses.size() > 1) {
                            outFile << "发现调用栈模式 " << pattern << " 对应 " << addresses.size() << " 个不同调用地址:\n";
                            for (const auto &addr: addresses) {
                                AddressInfo info = GetAddressInfo(addr);
                                outFile << "  " << FormatAddressInfo(info)
                                        << " 调用次数: " << std::dec << callSites.at(addr).callCount << "\n";
                            }
                            outFile << "\n";
                        }
                    }

                    outFile.close();
                    printf("已将分析数据保存到 %s\n", filename);
                }
            } catch (const std::exception &e) {
                printf("分析调用地址数据时出错: %s\n", e.what());
            }
        }

        // 清除调用地址数据
        void ClearCallSites(CallSitesMap &callSites) {
            callSites.clear();
            printf("已清除所有捕获的DrawCall调用数据\n");
        }
    }
}
