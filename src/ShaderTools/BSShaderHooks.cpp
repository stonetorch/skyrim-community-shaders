#include "BSShaderHooks.h"

namespace BSShaderHooks {
    // hk_LoadShaders 函数用于加载和注册着色器
    // 参数:
    // - bsShader: 指向 REX::BSShader 对象的指针
    // - std::uintptr_t: 未使用的参数
    void hk_LoadShaders(REX::BSShader *bsShader, std::uintptr_t) {
        // 记录日志，显示调用信息和像素着色器表的大小
        logger::info("BSShader::LoadShaders called on {} - ps count {}", bsShader->m_LoaderType,
                     bsShader->m_PixelShaderTable.size());

        // 如果加载器类型是 "Lighting"
        if (strcmp("Lighting", bsShader->m_LoaderType) == 0) {
            // 定义一个 unordered_map，用于存储技术 ID 和着色器文件路径的映射
            std::unordered_map<REX::TechniqueID, std::wstring> techniqueFileMap;

            // 获取当前路径并追加 "Data/SKSE/plugins/shaders/" 作为着色器目录
            const auto shaderDir = std::filesystem::current_path() /= "Data/SKSE/plugins/shaders/"sv;

            if (std::filesystem::exists(shaderDir)) {
                std::size_t foundCount = 0;
                std::size_t successCount = 0;
                std::size_t failedCount = 0;

                for (const auto &entry: std::filesystem::directory_iterator(shaderDir)) {
                    // 如果文件扩展名不是 ".hlsl"，则跳过
                    if (entry.path().extension().generic_string() != ".hlsl"sv)
                        continue;

                    // 提取文件名和技术 ID
                    // 技术ID是文件名第一个下划线前的内容
                    auto filenameStr = entry.path().filename().string();
                    auto techniqueIdStr = filenameStr.substr(0, filenameStr.find('_'));
                    const REX::TechniqueID techniqueId = std::strtoul(techniqueIdStr.c_str(), nullptr, 16);
                    logger::info("found shader technique id {:08x} with path {}", techniqueId,
                                 entry.path().generic_string());
                    foundCount++;
                    techniqueFileMap.insert(std::make_pair(techniqueId, absolute(entry.path()).wstring()));
                }

                // 遍历像素着色器表
                // bsShader应该是一个管理Shader的表格，有可能是单例？不过看hook的方式，可能存在多个bsShader对象，分管不同的领域
                // 这里遍历bsShader记录的PS表格，并覆盖所有能够替换的PS（根据之前创建的TechniqueID匹配，而TechniqueID是由某个神奇的算法算出来的，有点像hash）
                for (const auto &entry: bsShader->m_PixelShaderTable) {
                    auto tFileIt = techniqueFileMap.find(entry->m_TechniqueID);
                    if (tFileIt != techniqueFileMap.end()) {
                        // 编译并注册像素着色器
                        if (const auto shader = ShaderCompiler::CompileAndRegisterPixelShader(tFileIt->second)) {
                            logger::info("shader compiled successfully, replacing old shader");
                            successCount++;
                            entry->m_Shader = shader;
                        } else {
                            failedCount++;
                        }
                    } else {
                        logger::info("shader query not found, techniqueID {}", entry->m_TechniqueID);
                    }
                }
            }
        }

        // 定义另一个 unordered_map，用于存储技术 ID 和着色器文件路径的映射
        std::unordered_map<REX::TechniqueID, std::wstring> techniqueFileMap;

        // 获取当前路径并追加格式化的字符串作为新的着色器目录
        const auto shaderDir = std::filesystem::current_path() /= std::format(
                                   "Data\\Shaders\\{}\\"sv, bsShader->m_LoaderType);
        if (!std::filesystem::exists(shaderDir)) {
            return;
        }

        logger::info("{}", shaderDir.generic_string());
        // 初始化计数器
        std::size_t foundCount = 0;
        std::size_t successCount = 0;
        std::size_t failedCount = 0;

        // 遍历目录中的所有文件
        for (const auto &entry: std::filesystem::directory_iterator(shaderDir)) {
            std::string fileStr = entry.path().filename().generic_string();

            std::string techniqueIDStr;
            REX::TechniqueID techniqueId;
            if (fileStr.ends_with(".ps.hlsl")) {
                techniqueIDStr = fileStr.substr(0, fileStr.length() - 8);
                techniqueId = std::strtoul(techniqueIDStr.c_str(), nullptr, 16);
                auto tFileIt = techniqueFileMap.find(techniqueId);
                if (tFileIt != techniqueFileMap.end() && !tFileIt->second.ends_with(L".hlsl"))
                    continue; // 优先使用已编译的二进制文件
            } else if (fileStr.ends_with(".ps")) {
                techniqueIDStr = fileStr.substr(0, fileStr.length() - 3);
                techniqueId = std::strtoul(techniqueIDStr.c_str(), nullptr, 16);
                continue;
            } else {
                continue;
            }

            logger::info("found shader technique id {:08x} with path {}", techniqueId, entry.path().generic_string());
            foundCount++;
            techniqueFileMap.insert(std::make_pair(techniqueId, absolute(entry.path()).wstring()));
        }

        // 遍历像素着色器表
        for (const auto &entry: bsShader->m_PixelShaderTable) {
            auto tFileIt = techniqueFileMap.find(entry->m_TechniqueID);
            if (tFileIt != techniqueFileMap.end()) {
                bool compile = tFileIt->second.ends_with(L".hlsl");
                if (const auto shader = compile
                                            ? ShaderCompiler::CompileAndRegisterPixelShader(tFileIt->second)
                                            : ShaderCompiler::RegisterPixelShader(tFileIt->second)) {
                    logger::info("shader compiled successfully, replacing old shader");
                    successCount++;
                    entry->m_Shader = shader;
                } else {
                    failedCount++;
                }
            }
        }

        // 记录找到的着色器数量、成功替换的着色器数量和替换失败的着色器数量
        logger::info("found shaders: {} successfully replaced: {} failed to replace: {}", foundCount, successCount,
                     failedCount);
    }
}
