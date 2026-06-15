//
// Created by PC on 2026/6/15.
//

#ifndef RTFS2D_DESCRIPTOR_SETS_H
#define RTFS2D_DESCRIPTOR_SETS_H

#include <vulkan/vulkan_raii.hpp>

namespace rtfs2d {

enum DescriptorSetIndex {
    kSetAdvection,              // 平流
    kSetVorticity,              // 涡量约束
    kSetDiffusionEven,          // 扩散 u 分量偶次迭代
    kSetDiffusionOdd,           // 扩散 u 分量奇次迭代
    kSetDivergence,             // 散度计算
    kSetPressureEven,           // 压力求解偶次迭代
    kSetPressureOdd,            // 压力求解奇次迭代
    kSetProjection,             // 压力投影修正速度
    kSetIbmApplyForce,
    kSetIbmInterpolate,
    kSetIbmMask,
    kSetIbmSpreadMarkers,
    kSetSmoothVelocity,         // 平滑速度场（仅用于漩涡强度）
    kSetComputeScalarVI,        // 漩涡强度计算
    kSetComputeScalarOthers,    // 其他可视化标量计算
    kSetDyeAdvection1,          // 染料平流，用于实现染料可视化
    kSetDyeAdvection2,          // 染料平流 (翻转)
    kSetDyeSource1,             // 添加染料
    kSetDyeSource2,             // 添加染料 (翻转)
    kSetVis1,                   // 可视化
    kSetVis2,                   // 可视化 (翻转)
};

class DeviceManager;

namespace internal {
class DescriptorSetBuilder;
}

class DescriptorSets {
public:
    explicit DescriptorSets(DeviceManager& device);
    const vk::raii::DescriptorSet& SetAt(DescriptorSetIndex idx) const {
        return *sets_[static_cast<uint32_t>(idx)];
    }
    const vk::raii::DescriptorSetLayout& descriptor_set_layout() const { return *layout_; }
    const vk::raii::DescriptorPool& descriptor_pool() const { return *pool_; }
private:
    std::unique_ptr<vk::raii::DescriptorSetLayout> layout_;
    std::unique_ptr<vk::raii::DescriptorPool> pool_;
    std::vector<std::unique_ptr<vk::raii::DescriptorSet>> sets_;

    static void CreateDescriptorSets(internal::DescriptorSetBuilder& dsb);
    static std::vector<std::vector<uint32_t>> GetDescriptorRegistry();
};

namespace internal {

class DescriptorSetBuilder {
public:
    struct BuildResult {
        std::unique_ptr<vk::raii::DescriptorSetLayout> layout;
        std::unique_ptr<vk::raii::DescriptorPool> pool;
        std::vector<std::unique_ptr<vk::raii::DescriptorSet>> sets;
    };
    explicit DescriptorSetBuilder(DeviceManager& dm, std::vector<std::vector<uint32_t>> sets_registry);
    void CreateDescriptorSet(uint32_t binding, vk::ShaderStageFlags stages);
    BuildResult Build() const;

private:
    DeviceManager* dm_;
    std::vector<vk::DescriptorSetLayoutBinding> bindings_;
    std::vector<std::vector<uint32_t>> sets_registry_;
};

}

}



#endif //RTFS2D_DESCRIPTOR_SETS_H
