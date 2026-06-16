//
// Created by PC on 2026/6/15.
//

#include "descriptor_sets.h"
#include "device_manager.h"
#include "buffers.h"

namespace rtfs2d {

DescriptorSets::DescriptorSets(DeviceManager& device) {
    internal::DescriptorSetBuilder dsb(device, GetDescriptorRegistry());
    CreateDescriptorSets(dsb);
    auto [ds_layout, ds_pool, ds_sets] = dsb.Build();
    layout_ = std::move(ds_layout);
    pool_ = std::move(ds_pool);
    sets_ = std::move(ds_sets);
}

void DescriptorSets::CreateDescriptorSets(internal::DescriptorSetBuilder &dsb) {
    auto flag_frag = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eFragment;
    auto flag_compute = vk::ShaderStageFlagBits::eCompute;
    dsb.CreateDescriptorSet(kSetAdvection,          flag_compute);
    dsb.CreateDescriptorSet(kSetVorticity,          flag_compute);
    dsb.CreateDescriptorSet(kSetDiffusionEven,      flag_compute | flag_frag);
    dsb.CreateDescriptorSet(kSetDiffusionOdd,       flag_compute);
    dsb.CreateDescriptorSet(kSetDivergence,         flag_compute);
    dsb.CreateDescriptorSet(kSetPressureEven,       flag_compute);
    dsb.CreateDescriptorSet(kSetPressureOdd,        flag_compute);
    dsb.CreateDescriptorSet(kSetProjection,         flag_compute);
    dsb.CreateDescriptorSet(kSetIbmApplyForce,      flag_compute);
    dsb.CreateDescriptorSet(kSetIbmInterpolate,     flag_compute);
    dsb.CreateDescriptorSet(kSetIbmMask,            flag_compute);
    dsb.CreateDescriptorSet(kSetIbmSpreadMarkers,   flag_compute);
    dsb.CreateDescriptorSet(kSetSmoothVelocity,     flag_compute | flag_frag);
    dsb.CreateDescriptorSet(kSetComputeScalarVI,    flag_compute | flag_frag);
    dsb.CreateDescriptorSet(kSetComputeScalarOthers,flag_compute);
    dsb.CreateDescriptorSet(kSetDyeAdvection1,      flag_compute);
    dsb.CreateDescriptorSet(kSetDyeAdvection2,      flag_compute);
    dsb.CreateDescriptorSet(kSetDyeSource1,         flag_compute);
    dsb.CreateDescriptorSet(kSetDyeSource2,         flag_compute);
    dsb.CreateDescriptorSet(kSetVis1,               flag_frag);
    dsb.CreateDescriptorSet(kSetVis2,               flag_frag);
}

std::vector<std::vector<uint32_t>> DescriptorSets::GetDescriptorRegistry() {
    return {
        {kSetAdvection,
            0, buffers::kBufV0 /* u_src */,
            1, buffers::kBufV1 /* v_src */,
            2, buffers::kBufV2 /* u_dst */,
            3, buffers::kBufV3 /* v_dst */,
            5, buffers::kBufBc0,
            6, buffers::kBufBc1,
            7, buffers::kBufBc2,
            8, buffers::kBufBc3,
        },{kSetVorticity,
            0, buffers::kBufV2 /* u_src */,
            1, buffers::kBufV3 /* v_src */,
            5, buffers::kBufBc0,
            6, buffers::kBufBc1,
            7, buffers::kBufBc2,
            8, buffers::kBufBc3,
        },{kSetDiffusionEven,
            0, buffers::kBufV2 /* u_src */,
            1, buffers::kBufV3 /* v_src */,
            2, buffers::kBufV0 /* u_dst */,
            3, buffers::kBufV1 /* v_dst */,
            5, buffers::kBufBc0,
            6, buffers::kBufBc1,
            7, buffers::kBufBc2,
            8, buffers::kBufBc3,
        },{kSetDiffusionOdd,
            0, buffers::kBufV0 /* u_src */,
            1, buffers::kBufV1 /* v_src */,
            2, buffers::kBufV2 /* u_dst */,
            3, buffers::kBufV3 /* v_dst */,
            5, buffers::kBufBc0,
            6, buffers::kBufBc1,
            7, buffers::kBufBc2,
            8, buffers::kBufBc3,
        },{kSetDivergence,
            0, buffers::kBufV0 /* u_src */,
            1, buffers::kBufV1 /* v_src */,
            2, buffers::kBufV2 /*  div  */,
            5, buffers::kBufBc0,
            6, buffers::kBufBc1,
            7, buffers::kBufBc2,
            8, buffers::kBufBc3,
        },{kSetPressureEven,
            0, buffers::kBufV0 /* u_src */,
            1, buffers::kBufV1 /* v_src */,
            2, buffers::kBufV2 /*  div  */,
            3, buffers::kBufV4 /*  pi   */,
            4, buffers::kBufV3 /*  po   */,
            5, buffers::kBufBc0,
            6, buffers::kBufBc1,
            7, buffers::kBufBc2,
            8, buffers::kBufBc3,
        },{kSetPressureOdd,
            0, buffers::kBufV0,     // u_src
            1, buffers::kBufV1,     // v_src
            2, buffers::kBufV2,     // div
            3, buffers::kBufV3,     // pi
            4, buffers::kBufV4,     // po
            5, buffers::kBufBc0,
            6, buffers::kBufBc1,
            7, buffers::kBufBc2,
            8, buffers::kBufBc3,
        },{kSetProjection,
            0, buffers::kBufV0,     // u_src
            1, buffers::kBufV1,     // v_src
            2, buffers::kBufV4,     // p
            5, buffers::kBufBc0,
            6, buffers::kBufBc1,
            7, buffers::kBufBc2,
            8, buffers::kBufBc3,
        }, {kSetIbmApplyForce,
            0, buffers::kBufV0,     // u_src
            1, buffers::kBufV1,     // v_src
            11, buffers::kBufIbmForce,
            12, buffers::kBufIbmMask,
        }, {kSetIbmInterpolate,
            0, buffers::kBufV0,     // u_src
            1, buffers::kBufV1,     // v_src
            10, buffers::kBufIbmMarker,
        }, {kSetIbmMask,
            9, buffers::kBufIbmPolygon,
            12, buffers::kBufIbmMask,
        }, {kSetIbmSpreadMarkers,
            10, buffers::kBufIbmMarker,
            11, buffers::kBufIbmForce,
        }, {kSetSmoothVelocity,
            0, buffers::kBufV0,     // u_src
            1, buffers::kBufV1,     // v_src
            3, buffers::kBufV3,     // u_sm
            4, buffers::kBufV5,     // v_sm
            12, buffers::kBufIbmMask,
        }, {kSetComputeScalarVI,
            0, buffers::kBufV3,     // u_sm
            1, buffers::kBufV5,     // v_sm
            2, buffers::kBufV2,     // scalar
            3, buffers::kBufV3,     // p (占位)
            12, buffers::kBufIbmMask,
        }, {kSetComputeScalarOthers,
            0, buffers::kBufV0,     // u_src
            1, buffers::kBufV1,     // v_src
            2, buffers::kBufV2,     // scala
            3, buffers::kBufV3,     // p
            12, buffers::kBufIbmMask,
        }, {kSetDyeAdvection1,
            0, buffers::kBufV2,     // u_src
            1, buffers::kBufV3,     // v_src
            12, buffers::kBufIbmMask,
            13, buffers::kBufV5,    // dye_src
            14, buffers::kBufV6,    // dye_dst
            5, buffers::kBufBc0,
            6, buffers::kBufBc1,
            7, buffers::kBufBc2,
            8, buffers::kBufBc3,
        }, {kSetDyeAdvection2,
            0, buffers::kBufV2,     // u_src
            1, buffers::kBufV3,     // v_src
            12, buffers::kBufIbmMask,
            13, buffers::kBufV6,    // dye_src
            14, buffers::kBufV5,    // dye_dst
            5, buffers::kBufBc0,
            6, buffers::kBufBc1,
            7, buffers::kBufBc2,
            8, buffers::kBufBc3,
        }, {kSetDyeSource1,
            12, buffers::kBufIbmMask,
            13, buffers::kBufV6     // dye_dst
        } ,{kSetDyeSource2,
            12, buffers::kBufIbmMask,
            13, buffers::kBufV5     // dye_dst
        } ,{kSetVis1,
            2, buffers::kBufV2,     // scalar
            12, buffers::kBufIbmMask,
            13, buffers::kBufV6     // dye_dst
        }, {kSetVis2,
            2, buffers::kBufV2,     // scalar
            12, buffers::kBufIbmMask,
            13, buffers::kBufV5     // dye_dst
        },
    };
}

namespace internal {



DescriptorSetBuilder::DescriptorSetBuilder(DeviceManager& dm, std::vector<std::vector<uint32_t>> sets_registry)
        : dm_(&dm), sets_registry_(std::move(sets_registry)) {
}

void DescriptorSetBuilder::CreateDescriptorSet(
        uint32_t binding, vk::ShaderStageFlags stages) {
    vk::DescriptorSetLayoutBinding lb{};
    lb.setBinding(binding)
        .setDescriptorType(vk::DescriptorType::eStorageBuffer)
        .setDescriptorCount(1)
        .setStageFlags(stages);
    bindings_.push_back(lb);
}

DescriptorSetBuilder::BuildResult DescriptorSetBuilder::Build() const {
    if (sets_registry_.size() != bindings_.size()) {
        throw std::runtime_error("DescriptorSetBuilder::Build: sets_registry_.size() != bindings_.size()");
    }
    uint32_t sets_count = sets_registry_.size();
    uint32_t max_descriptor_count = 0;
    for (const auto& reg : sets_registry_) {
        auto it = reg.begin();
        ++it;
        while (it != reg.end()) {
            max_descriptor_count = std::max(max_descriptor_count, *it + 1);
            it += 2;
        }
    }
    // 创建描述符池
    vk::DescriptorPoolSize ps{};
    ps.setType(vk::DescriptorType::eStorageBuffer)
        .setDescriptorCount(max_descriptor_count * sets_count);
    vk::DescriptorPoolCreateInfo pool_ci{};
    pool_ci.setPoolSizeCount(1)
        .setPPoolSizes(&ps)
        .setMaxSets(sets_count)
        .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
    auto pool = std::make_unique<vk::raii::DescriptorPool>(
        dm_->device().createDescriptorPool(pool_ci));

    //创建描述符集布局
    vk::DescriptorSetLayoutCreateInfo layout_ci{};
    layout_ci.setBindingCount(max_descriptor_count)
        .setPBindings(bindings_.data());
    auto layout = std::make_unique<vk::raii::DescriptorSetLayout>(
        dm_->device().createDescriptorSetLayout(layout_ci));

    // 创建描述符
    std::vector<std::unique_ptr<vk::raii::DescriptorSet>> sets(sets_count);
    for (const auto& set_reg : sets_registry_) {
        auto it = set_reg.begin();
        uint32_t set_idx = *it++;

        vk::DescriptorSetAllocateInfo ai{};
        ai.setDescriptorSetCount(1)
            .setDescriptorPool(**pool)
            .setSetLayouts({**layout});
        auto temp_sets = dm_->device().allocateDescriptorSets(ai);
        sets[set_idx] = std::make_unique<vk::raii::DescriptorSet>(std::move(temp_sets[0]));

        // 绑定缓冲区
        while (it != set_reg.end()) {
            uint32_t binding = *it++;
            uint32_t buffer = *it++;
            vk::DescriptorBufferInfo bi{};
            bi.setBuffer(*dm_->BufferAt(buffer))
                .setOffset(0)
                .setRange(VK_WHOLE_SIZE);
            vk::WriteDescriptorSet wds{};
            wds.setDstSet(*sets[set_idx])
                .setDstBinding(binding)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setPBufferInfo(&bi);
            dm_->device().updateDescriptorSets(wds, nullptr);
        }
    }
    return {std::move(layout), std::move(pool), std::move(sets)};
}

}

}
