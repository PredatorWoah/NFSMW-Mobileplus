// Game-free on-device arithmetic check. argv[1] is bc_decode_test's shader,
// compiled to SPIR-V on the host. This does not validate untile/image sampling.
#include <gpu/bc_decode_glsl.h>
#include <vulkan/vulkan.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <vector>

static void Check(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) {
        std::fprintf(stderr, "%s failed: %d\n", operation, int(result));
        throw std::runtime_error(operation);
    }
}

static uint32_t Swap(uint32_t v, uint32_t endian) {
    if (endian==1) return ((v<<8)&0xFF00FF00u)|((v>>8)&0x00FF00FFu);
    if (endian==2) return __builtin_bswap32(v);
    if (endian==3) return (v>>16)|(v<<16);
    return v;
}
static uint32_t Tile(uint32_t x,uint32_t y,uint32_t pitch,uint32_t log) {
    uint32_t outer=(((y>>5)*(pitch>>5))+(x>>5))<<6;
    uint32_t inner=(((y>>1)&7)<<3)|(x&7), oi=(outer|inner)<<log;
    uint32_t bank=(y>>4)&1,pipe=((x>>3)&3)^(((y>>3)&1)<<1);
    return ((y&1)<<4)|(pipe<<6)|(bank<<11)|(oi&15)|(((oi>>4)&1)<<5)|(((oi>>5)&7)<<8)|((oi>>8)<<12);
}

int main(int argc, char** argv) try {
    const bool untile=argc==3 && std::string_view(argv[2])=="--untile";
    if (argc != 2 && !untile) { std::fprintf(stderr,"usage: android-bc-decode-test shader.spv [--untile]\n"); return 2; }
    std::ifstream file(argv[1],std::ios::binary|std::ios::ate);
    const auto size=file.tellg();
    if (!file || size <= 0 || size % 4) throw std::runtime_error("invalid SPIR-V file");
    std::vector<uint32_t> spirv(size_t(size)/4);
    file.seekg(0); file.read(reinterpret_cast<char*>(spirv.data()),size);
    if (!file) throw std::runtime_error("short SPIR-V read");
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.apiVersion=VK_API_VERSION_1_1;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ici.pApplicationInfo=&app;
    VkInstance instance; Check(vkCreateInstance(&ici,nullptr,&instance),"instance");
    uint32_t count=0; Check(vkEnumeratePhysicalDevices(instance,&count,nullptr),"device count");
    if (!count) throw std::runtime_error("no GPU");
    std::vector<VkPhysicalDevice> physical(count);
    Check(vkEnumeratePhysicalDevices(instance,&count,physical.data()),"devices");
    VkPhysicalDevice gpu=physical[0];
    VkPhysicalDeviceProperties props{}; vkGetPhysicalDeviceProperties(gpu,&props);
    std::printf("GPU arithmetic test: %s; no game code\n",props.deviceName);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu,&count,nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu,&count,families.data());
    uint32_t family=UINT32_MAX;
    for (uint32_t i=0;i<count;++i) if(families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {family=i;break;}
    if(family==UINT32_MAX) throw std::runtime_error("no compute queue");
    float priority=1;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex=family;qci.queueCount=1;qci.pQueuePriorities=&priority;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dci.queueCreateInfoCount=1;dci.pQueueCreateInfos=&qci;
    VkDevice device;Check(vkCreateDevice(gpu,&dci,nullptr,&device),"logical device");
    VkQueue queue;vkGetDeviceQueue(device,family,0,&queue);
    constexpr uint32_t blocks=4096, inputWords=blocks*5, pixels=blocks*16;
    const VkDeviceSize bytes=untile ? 96ull*262144 : VkDeviceSize(inputWords+pixels)*4;
    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bci.size=bytes;bci.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VkBuffer buffer;Check(vkCreateBuffer(device,&bci,nullptr,&buffer),"buffer");
    VkMemoryRequirements req{};vkGetBufferMemoryRequirements(device,buffer,&req);
    VkPhysicalDeviceMemoryProperties mem{};vkGetPhysicalDeviceMemoryProperties(gpu,&mem);
    uint32_t type=UINT32_MAX;
    for(uint32_t i=0;i<mem.memoryTypeCount;++i)
        if((req.memoryTypeBits&(1u<<i)) && (mem.memoryTypes[i].propertyFlags &
           (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
           (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {type=i;break;}
    if(type==UINT32_MAX) throw std::runtime_error("no coherent host memory");
    VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};mai.allocationSize=req.size;mai.memoryTypeIndex=type;
    VkDeviceMemory memory;Check(vkAllocateMemory(device,&mai,nullptr,&memory),"memory");
    Check(vkBindBufferMemory(device,buffer,memory,0),"bind");
    void* mapped=nullptr;Check(vkMapMemory(device,memory,0,bytes,0,&mapped),"map");
    auto* data=static_cast<uint32_t*>(mapped);
    struct Dispatch {std::array<uint32_t,14> push{};uint32_t x=1,y=1;};
    struct Expected {uint32_t word,value;};
    std::vector<Dispatch> dispatches;
    std::vector<Expected> expected;
    uint32_t seed=9500;
    const auto random=[&]() {seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;};
    std::fill_n(data,size_t(bytes/4),0xDEADBEEFu);
    if (untile) {
        // These independent source regions model packed offsets, separate cube
        // faces/mips and both guest layouts. Small cases cover sub-4x4 mips;
        // 33x35 blocks cross Xenos tile boundaries in both axes.
        for(uint32_t kind=5;kind<=8;++kind) for(uint32_t endian=0;endian<4;++endian)
            for(uint32_t tiled=0;tiled<2;++tiled)
                for(auto dimensions : {std::array<uint32_t,2>{1,1},{5,3},{33,35}}) {
                    uint32_t bw=dimensions[0],bh=dimensions[1],log=kind==5?3:4;
                    uint32_t base=uint32_t(dispatches.size())*262144, dest=(base+131072)/4+16;
                    uint32_t pitch=tiled?64:64u<<log;
                    Dispatch dispatch;
                    dispatch.push={base,tiled,pitch,log,1,3,bw,bh,endian,kind,dest,bw*4,0,64};
                    dispatch.x=(bw+7)/8;dispatch.y=(bh+7)/8;
                    dispatches.push_back(dispatch);
                    for(uint32_t by=0;by<bh;++by) for(uint32_t bx=0;bx<bw;++bx) {
                        uint32_t words[4]={random(),random(),random(),random()};
                        uint32_t address=base+(tiled?Tile(bx+1,by+3,64,log):(by+3)*pitch+((bx+1)<<log));
                        for(uint32_t w=0;w<(1u<<(log-2));++w)data[address/4+w]=Swap(words[w],endian);
                        for(uint32_t t=0;t<16;++t) {
                            uint32_t at=dest+(by*4+t/4)*(bw*4)+bx*4+(t&3);
                            expected.push_back({at,gpu::bc::sbBcPixel(kind,words[0],words[1],words[2],words[3],t)});
                        }
                    }
                    for(uint32_t i=0;i<16;++i) {
                        expected.push_back({dest-16+i,0xDEADBEEFu});
                        expected.push_back({dest+bw*bh*16+i,0xDEADBEEFu});
                    }
                }
    } else {
        for(uint32_t b=0;b<blocks;++b) {
            data[b*5]=5+b%4;
            for(uint32_t w=1;w<=4;++w)data[b*5+w]=random();
            for(uint32_t t=0;t<16;++t)
                expected.push_back({inputWords+b*16+t,gpu::bc::sbBcPixel(data[b*5],data[b*5+1],data[b*5+2],data[b*5+3],data[b*5+4],t)});
        }
        Dispatch dispatch;dispatch.push[0]=blocks;dispatch.x=(pixels+63)/64;dispatches.push_back(dispatch);
    }
    VkShaderModuleCreateInfo sci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sci.codeSize=size_t(size);sci.pCode=spirv.data();
    VkShaderModule shader;Check(vkCreateShaderModule(device,&sci,nullptr,&shader),"shader");
    VkDescriptorSetLayoutBinding bindings[2]={{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
        {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}};
    VkDescriptorSetLayoutCreateInfo slci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};slci.bindingCount=2;slci.pBindings=bindings;
    VkDescriptorSetLayout setLayout;Check(vkCreateDescriptorSetLayout(device,&slci,nullptr,&setLayout),"set layout");
    VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT,0,56};
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};plci.setLayoutCount=1;plci.pSetLayouts=&setLayout;plci.pushConstantRangeCount=1;plci.pPushConstantRanges=&range;
    VkPipelineLayout layout;Check(vkCreatePipelineLayout(device,&plci,nullptr,&layout),"pipeline layout");
    VkComputePipelineCreateInfo cpci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};cpci.layout=layout;
    cpci.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_COMPUTE_BIT,shader,"main",nullptr};
    VkPipeline pipeline;Check(vkCreateComputePipelines(device,VK_NULL_HANDLE,1,&cpci,nullptr,&pipeline),"compute pipeline");
    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2};
    VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dpci.maxSets=1;dpci.poolSizeCount=1;dpci.pPoolSizes=&poolSize;
    VkDescriptorPool pool;Check(vkCreateDescriptorPool(device,&dpci,nullptr,&pool),"descriptor pool");
    VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};dsai.descriptorPool=pool;dsai.descriptorSetCount=1;dsai.pSetLayouts=&setLayout;
    VkDescriptorSet set;Check(vkAllocateDescriptorSets(device,&dsai,&set),"descriptor set");
    VkDescriptorBufferInfo bi{buffer,0,bytes};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=set;write.dstBinding=0;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;write.pBufferInfo=&bi;
    vkUpdateDescriptorSets(device,1,&write,0,nullptr);
    write.dstBinding=1;vkUpdateDescriptorSets(device,1,&write,0,nullptr);
    VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cpi.queueFamilyIndex=family;
    VkCommandPool commandPool;Check(vkCreateCommandPool(device,&cpi,nullptr,&commandPool),"command pool");
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};cai.commandPool=commandPool;cai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;cai.commandBufferCount=1;
    VkCommandBuffer cmd;Check(vkAllocateCommandBuffers(device,&cai,&cmd),"command buffer");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};Check(vkBeginCommandBuffer(cmd,&begin),"begin");
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
    vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,layout,0,1,&set,0,nullptr);
    for(const auto& dispatch:dispatches) {
        vkCmdPushConstants(cmd,layout,VK_SHADER_STAGE_COMPUTE_BIT,0,56,dispatch.push.data());
        vkCmdDispatch(cmd,dispatch.x,dispatch.y,1);
    }
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);
    Check(vkEndCommandBuffer(cmd),"end");
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};VkFence fence;Check(vkCreateFence(device,&fci,nullptr,&fence),"fence");
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&cmd;
    Check(vkQueueSubmit(queue,1,&submit,fence),"submit");
    Check(vkWaitForFences(device,1,&fence,VK_TRUE,10000000000ull),"10s completion fence");
    uint32_t failures=0;
    for(const auto& check:expected) {
        if(data[check.word]!=check.value) {
            if(failures<8)std::printf("Mismatch word %u: %08x expected %08x\n",check.word,data[check.word],check.value);
            ++failures;
        }
    }
    std::printf("BC1/2/3/5 GPU %s: %zu dispatches, %zu checked words, %u mismatches; %s\n",
        untile?"actual untile/endian/offset path":"arithmetic",dispatches.size(),expected.size(),failures,failures?"FAIL":"PASS");
    vkDestroyFence(device,fence,nullptr);vkDestroyCommandPool(device,commandPool,nullptr);
    vkDestroyDescriptorPool(device,pool,nullptr);vkDestroyPipeline(device,pipeline,nullptr);
    vkDestroyPipelineLayout(device,layout,nullptr);vkDestroyDescriptorSetLayout(device,setLayout,nullptr);
    vkDestroyShaderModule(device,shader,nullptr);vkUnmapMemory(device,memory);
    vkDestroyBuffer(device,buffer,nullptr);vkFreeMemory(device,memory,nullptr);
    vkDestroyDevice(device,nullptr);vkDestroyInstance(instance,nullptr);
    return failures?1:0;
} catch(const std::exception& e) {std::fprintf(stderr,"BC GPU test failed: %s\n",e.what());return 1;}
