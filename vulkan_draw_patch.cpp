void VulkanDevice::DrawFullscreenPass(PipelineHandle pipeline, std::span<const TextureHandle> inputs,
                                       TextureHandle output,
                                       const std::unordered_map<std::string, float>& uniformValues) {
    // Record into current command buffer
    VkCommandBuffer cmd = commandBuffers_[currentFrame_];

    // Get pipeline resource
    VkPipelineResource* pipelineRes = pipelines_.Get(pipeline);
    if (!pipelineRes) return;

    // Get output texture
    VkTextureResource* outputRes = textures_.Get(output);
    if (!outputRes) return;

    // Get per-frame uniform buffers and descriptor sets
    FrameUniformBuffers& frameBuffers = frameUniformBuffers_[currentFrame_];

    // Update uniform buffer (Set 0): projection matrix + resolution
    struct UniformData {
        float projection[16]; // mat4 column-major
        float resolution[2];
        float _pad[2];
    } uniformData;

    // Orthographic projection for fullscreen triangle (NDC -> UV mapping)
    // Identity projection since vertex shader outputs NDC directly
    for (int i = 0; i < 16; ++i) uniformData.projection[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    uniformData.resolution[0] = static_cast<float>(outputRes->width);
    uniformData.resolution[1] = static_cast<float>(outputRes->height);

    VkBufferResource* uniformBufRes = buffers_.Get(frameBuffers.uniformBuffer);
    if (uniformBufRes && uniformBufRes->mapped) {
        std::memcpy(uniformBufRes->mapped, &uniformData, sizeof(uniformData));
    }

    // Update descriptor set 0 (uniforms) - bind uniform buffer
    VkDescriptorBufferInfo uniformBufferInfo{};
    uniformBufferInfo.buffer = uniformBufRes ? uniformBufRes->buffer : VK_NULL_HANDLE;
    uniformBufferInfo.offset = 0;
    uniformBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet uniformWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    uniformWrite.dstSet = frameBuffers.uniformSet;
    uniformWrite.dstBinding = 0;
    uniformWrite.descriptorCount = 1;
    uniformWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformWrite.pBufferInfo = &uniformBufferInfo;
    vkUpdateDescriptorSets(device_, 1, &uniformWrite, 0, nullptr);

    // Update param buffer (Set 1) with animated uniform values
    VkBufferResource* paramBufRes = buffers_.Get(frameBuffers.paramBuffer);
    if (paramBufRes && paramBufRes->mapped) {
        // Write uniform values to param buffer
        // We'll pack them as a simple array of floats, with a name-to-offset mapping
        // For simplicity, use a fixed layout: up to 32 floats (128 bytes)
        float paramData[32] = {0};
        int idx = 0;
        for (const auto& [name, value] : uniformValues) {
            if (idx < 32) {
                paramData[idx++] = value;
            }
        }
        std::memcpy(paramBufRes->mapped, paramData, sizeof(paramData));
    }

    VkDescriptorBufferInfo paramBufferInfo{};
    paramBufferInfo.buffer = paramBufRes ? paramBufRes->buffer : VK_NULL_HANDLE;
    paramBufferInfo.offset = 0;
    paramBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet paramWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    paramWrite.dstSet = frameBuffers.paramSet;
    paramWrite.dstBinding = 0;
    paramWrite.descriptorCount = 1;
    paramWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    paramWrite.pBufferInfo = &paramBufferInfo;
    vkUpdateDescriptorSets(device_, 1, &paramWrite, 0, nullptr);

    // Update descriptor set 2 (textures) - bind input textures
    // For simplicity, we'll bind up to 2 textures to the pre-allocated texture set
    // (which uses textureSetLayout2_ - 2 combined image samplers)
    VkDescriptorImageInfo imageInfos[2] = {};
    uint32_t boundCount = 0;
    for (uint32_t i = 0; i < inputs.size() && i < 2; ++i) {
        VkTextureResource* texRes = textures_.Get(inputs[i]);
        if (texRes) {
            imageInfos[boundCount].sampler = texRes->ycbcrSampler ? texRes->ycbcrSampler : VK_NULL_HANDLE;
            imageInfos[boundCount].imageView = texRes->view;
            imageInfos[boundCount].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            boundCount++;
        }
    }

    if (boundCount > 0) {
        VkWriteDescriptorSet textureWrites[2]{};
        for (uint32_t i = 0; i < boundCount; ++i) {
            textureWrites[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            textureWrites[i].dstSet = frameBuffers.textureSet;
            textureWrites[i].dstBinding = i;
            textureWrites[i].descriptorCount = 1;
            textureWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            textureWrites[i].pImageInfo = &imageInfos[i];
        }
        vkUpdateDescriptorSets(device_, boundCount, textureWrites, 0, nullptr);
    }

    // Bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineRes->pipeline);

    // Bind descriptor sets (Set 0, 1, 2)
    VkDescriptorSet sets[3] = {
        frameBuffers.uniformSet,
        frameBuffers.paramSet,
        frameBuffers.textureSet
    };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphPipelineLayout_,
                            0, 3, sets, 0, nullptr);

    // Set dynamic viewport/scissor for output texture
    VkViewport viewport{0, 0, static_cast<float>(outputRes->width),
                         static_cast<float>(outputRes->height), 0.0f, 1.0f};
    VkRect2D scissor{{0, 0}, {outputRes->width, outputRes->height}};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Transition output to color attachment optimal
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.image = outputRes->image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Begin dynamic rendering
    VkRenderingAttachmentInfo colorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttachment.imageView = outputRes->view;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
    renderingInfo.renderArea = {{0, 0}, {outputRes->width, outputRes->height}};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    vkCmdBeginRendering(cmd, &renderingInfo);

    // Draw fullscreen triangle (3 vertices)
    vkCmdDraw(cmd, 3, 1, 0, 0);

    lastFrameStats_.drawCalls++;

    vkCmdEndRendering(cmd);

    // Transition output to shader read optimal for next pass
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
}