#include "svulkan2/resource/material.h"
#include "svulkan2/core/buffer.h"
#include "svulkan2/core/context.h"

namespace svulkan2 {
namespace resource {

static void updateDescriptorSets(
    vk::Device device, vk::DescriptorSet descriptorSet,
    std::vector<std::tuple<vk::DescriptorType, vk::Buffer,
                           vk::BufferView>> const &bufferData,
    std::vector<std::tuple<vk::ImageView, vk::Sampler>> textureData,
    uint32_t bindingOffset) {

  std::vector<vk::DescriptorBufferInfo> bufferInfos;
  bufferInfos.reserve(bufferData.size());

  std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
  writeDescriptorSets.reserve(bufferData.size() + textureData.size() ? 1 : 0);

  uint32_t dstBinding = bindingOffset;
  for (auto const &bd : bufferData) {
    bufferInfos.push_back(
        vk::DescriptorBufferInfo(std::get<1>(bd), 0, VK_WHOLE_SIZE));
    writeDescriptorSets.push_back(vk::WriteDescriptorSet(
        descriptorSet, dstBinding++, 0, 1, std::get<0>(bd), nullptr,
        &bufferInfos.back(), std::get<2>(bd) ? &std::get<2>(bd) : nullptr));
  }

  std::vector<vk::DescriptorImageInfo> imageInfos;
  for (auto const &tex : textureData) {
    imageInfos.push_back(
        vk::DescriptorImageInfo(std::get<1>(tex), std::get<0>(tex),
                                vk::ImageLayout::eShaderReadOnlyOptimal));
  }
  if (imageInfos.size()) {
    writeDescriptorSets.push_back(vk::WriteDescriptorSet(
        descriptorSet, dstBinding, 0, imageInfos.size(),
        vk::DescriptorType::eCombinedImageSampler, imageInfos.data()));
  }
  device.updateDescriptorSets(writeDescriptorSets, nullptr);
}

void SVMetallicMaterial::setTextures(
    std::shared_ptr<SVTexture> baseColorTexture,
    std::shared_ptr<SVTexture> roughnessTexture,
    std::shared_ptr<SVTexture> normalTexture,
    std::shared_ptr<SVTexture> metallicTexture) {
  mDirty = true;

  mBaseColorTexture = baseColorTexture;
  mRoughnessTexture = roughnessTexture;
  mNormalTexture = normalTexture;
  mMetallicTexture = metallicTexture;

  if (mBaseColorTexture) {
    setBit(mBuffer.textureMask, 0);
  } else {
    unsetBit(mBuffer.textureMask, 0);
  }

  if (mRoughnessTexture) {
    setBit(mBuffer.textureMask, 1);
  } else {
    unsetBit(mBuffer.textureMask, 1);
  }

  if (mNormalTexture) {
    setBit(mBuffer.textureMask, 2);
  } else {
    unsetBit(mBuffer.textureMask, 2);
  }

  if (mMetallicTexture) {
    setBit(mBuffer.textureMask, 3);
  } else {
    unsetBit(mBuffer.textureMask, 3);
  }
}

void SVSpecularMaterial::setTextures(std::shared_ptr<SVTexture> diffuseTexture,
                                     std::shared_ptr<SVTexture> specularTexture,
                                     std::shared_ptr<SVTexture> normalTexture) {
  mDirty = true;
  mDiffuseTexture = diffuseTexture;
  mSpecularTexture = specularTexture;
  mNormalTexture = normalTexture;

  if (diffuseTexture) {
    setBit(mBuffer.textureMask, 0);
  } else {
    unsetBit(mBuffer.textureMask, 0);
  }

  if (specularTexture) {
    setBit(mBuffer.textureMask, 1);
  } else {
    unsetBit(mBuffer.textureMask, 1);
  }

  if (normalTexture) {
    setBit(mBuffer.textureMask, 2);
  } else {
    unsetBit(mBuffer.textureMask, 2);
  }
}

void SVMetallicMaterial::uploadToDevice(core::Context &context) {
  if (!mDirty) {
    return;
  }
  if (!mDeviceBuffer) {
    mDeviceBuffer = context.getAllocator().allocateUniformBuffer(
        sizeof(SVMetallicMaterial::Buffer));
    auto layout = context.getMetallicDescriptorSetLayout();
    mDescriptorSet = std::move(
        context.getDevice()
            .allocateDescriptorSetsUnique(vk::DescriptorSetAllocateInfo(
                context.getDescriptorPool(), 1, &layout))
            .front());
  }
  auto defaultTexture = context.getResourceManager().getDefaultTexture();
  std::vector<std::tuple<vk::ImageView, vk::Sampler>> textures;
  if (mBaseColorTexture) {
    mBaseColorTexture->uploadToDevice(context);
    textures.push_back(
        {mBaseColorTexture->getImageView(), mBaseColorTexture->getSampler()});
  } else {
    defaultTexture->uploadToDevice(context);
    textures.push_back(
        {defaultTexture->getImageView(), defaultTexture->getSampler()});
  }
  if (mNormalTexture) {
    mNormalTexture->uploadToDevice(context);
    textures.push_back(
        {mNormalTexture->getImageView(), mNormalTexture->getSampler()});
  } else {
    defaultTexture->uploadToDevice(context);
    textures.push_back(
        {defaultTexture->getImageView(), defaultTexture->getSampler()});
  }
  if (mRoughnessTexture) {
    mRoughnessTexture->uploadToDevice(context);
    textures.push_back(
        {mRoughnessTexture->getImageView(), mRoughnessTexture->getSampler()});
  } else {
    defaultTexture->uploadToDevice(context);
    textures.push_back(
        {defaultTexture->getImageView(), defaultTexture->getSampler()});
  }
  if (mMetallicTexture) {
    mMetallicTexture->uploadToDevice(context);
    textures.push_back(
        {mMetallicTexture->getImageView(), mMetallicTexture->getSampler()});
  } else {
    defaultTexture->uploadToDevice(context);
    textures.push_back(
        {defaultTexture->getImageView(), defaultTexture->getSampler()});
  }
  mDeviceBuffer->upload(mBuffer);
  updateDescriptorSets(context.getDevice(), mDescriptorSet.get(),
                       {{vk::DescriptorType::eUniformBuffer,
                         mDeviceBuffer->getVulkanBuffer(), nullptr}},
                       textures, 0);
  mDirty = false;
}

void SVSpecularMaterial::uploadToDevice(core::Context &context) {
  if (!mDirty) {
    return;
  }
  if (!mDeviceBuffer) {
    mDeviceBuffer = context.getAllocator().allocateUniformBuffer(
        sizeof(SVSpecularMaterial::Buffer));
    auto layout = context.getSpecularDescriptorSetLayout();
    mDescriptorSet = std::move(
        context.getDevice()
            .allocateDescriptorSetsUnique(vk::DescriptorSetAllocateInfo(
                context.getDescriptorPool(), 1, &layout))
            .front());
  }
  auto defaultTexture = context.getResourceManager().getDefaultTexture();
  std::vector<std::tuple<vk::ImageView, vk::Sampler>> textures;
  if (mDiffuseTexture) {
    mDiffuseTexture->uploadToDevice(context);
    textures.push_back(
        {mDiffuseTexture->getImageView(), mDiffuseTexture->getSampler()});
  } else {
    defaultTexture->uploadToDevice(context);
    textures.push_back(
        {defaultTexture->getImageView(), defaultTexture->getSampler()});
  }
  if (mNormalTexture) {
    mNormalTexture->uploadToDevice(context);
    textures.push_back(
        {mNormalTexture->getImageView(), mNormalTexture->getSampler()});
  } else {
    defaultTexture->uploadToDevice(context);
    textures.push_back(
        {defaultTexture->getImageView(), defaultTexture->getSampler()});
  }
  if (mSpecularTexture) {
    mSpecularTexture->uploadToDevice(context);
    textures.push_back(
        {mSpecularTexture->getImageView(), mSpecularTexture->getSampler()});
  } else {
    defaultTexture->uploadToDevice(context);
    textures.push_back(
        {defaultTexture->getImageView(), defaultTexture->getSampler()});
  }

  mDeviceBuffer->upload(mBuffer);
  updateDescriptorSets(context.getDevice(), mDescriptorSet.get(),
                       {{vk::DescriptorType::eUniformBuffer,
                         mDeviceBuffer->getVulkanBuffer(), nullptr}},
                       {}, 0);
  updateDescriptorSets(context.getDevice(), mDescriptorSet.get(),
                       {{vk::DescriptorType::eUniformBuffer,
                         mDeviceBuffer->getVulkanBuffer(), nullptr}},
                       textures, 0);
  mDirty = false;
}

} // namespace resource
} // namespace svulkan2