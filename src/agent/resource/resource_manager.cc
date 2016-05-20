
// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "resource_manager.h"

#include "protocol/galaxy.pb.h"
#include "cpu_resource.h"
#include "memory_resource.h"
#include "volum_resource.h"
#include <boost/thread/lock_guard.hpp>

namespace baidu {
namespace galaxy {
namespace resource {
ResourceManager::ResourceManager() :
    cpu_(new CpuResource()),
    memory_(new MemoryResource()),
    volum_(new VolumResource())
{

}

ResourceManager::~ResourceManager()
{

}

int ResourceManager::Load()
{
    if (0 != cpu_->Load()) {
        return -1;
    }

    if (0 != memory_->Load()) {
        return -1;
    }

    if (0 != volum_->Load()) {
        return -1;
    }
    return 0;
}

std::string ResourceManager::ToString()
{
    return "";
}

int ResourceManager::Allocate(const baidu::galaxy::proto::ContainerDescription& desc)
{

    int64_t memroy_require = 0;
    int64_t cpu_millicores = 0;
    std::vector<const baidu::galaxy::proto::VolumRequired*> vv;
    CalResource(desc, cpu_millicores, memroy_require, vv);

    boost::mutex::scoped_lock lock(mutex_);

    // allocate cpu
    if (0 != cpu_->Allocate(cpu_millicores)) {
        return -1;
    }

    if (0 != memory_->Allocate(memroy_require)) {
        cpu_->Release(cpu_millicores);
        return -1;
    }

    if (0 != this->Allocate(vv)) {
        cpu_->Release(cpu_millicores);
        memory_->Release(memroy_require);
        return -1;
    }
    return 0;
}

int ResourceManager::Allocate(std::vector<const baidu::galaxy::proto::VolumRequired*>& vv)
{

    size_t m = 0;
    for (; m < vv.size(); m++) {
        if (0 != volum_->Allocat(*vv[m])) {
            break;
        }
    }

    if (m != vv.size()) {
        for (size_t i = 0; i < m; i++) {
            volum_->Release(*vv[i]);
        }
        return -1;
    }
    return 0;
}



int ResourceManager::Release(const baidu::galaxy::proto::ContainerDescription& desc)
{
    int64_t memroy_require = 0;
    int64_t cpu_millicores = 0;
    std::vector<const baidu::galaxy::proto::VolumRequired*> vv;
    CalResource(desc, cpu_millicores, memroy_require, vv);

    boost::mutex::scoped_lock lock(mutex_);
    int ret = cpu_->Release(cpu_millicores);
    assert(0 == ret);

    ret = memory_->Release(memroy_require);
    assert(0 == ret);

    for (size_t i = 0; i < vv.size(); i++) {
        ret = volum_->Release(*vv[i]);
        assert(0 == ret);
    }
    return 0;
}

int ResourceManager::Resource(boost::shared_ptr<void> resource)
{
    assert(0);
    return -1;
}

boost::shared_ptr<baidu::galaxy::proto::Resource> ResourceManager::GetCpuResource()
{
    uint64_t total = 0;
    uint64_t assigned = 0;
    cpu_->Resource(total, assigned);
    boost::shared_ptr<baidu::galaxy::proto::Resource> ret(new baidu::galaxy::proto::Resource);
    ret->set_total(total);
    ret->set_assigned(assigned);

    return ret;
}

boost::shared_ptr<baidu::galaxy::proto::Resource> ResourceManager::GetMemoryResource()
{
    uint64_t total = 0;
    uint64_t assigned = 0;
    memory_->Resource(total, assigned);
    boost::shared_ptr<baidu::galaxy::proto::Resource> ret(new baidu::galaxy::proto::Resource);
    ret->set_total(total);
    ret->set_assigned(assigned);
    return ret;
}
void ResourceManager::GetVolumResource(std::vector<boost::shared_ptr<baidu::galaxy::proto::VolumResource> >& resource)
{
    std::map<std::string, baidu::galaxy::resource::VolumResource::Volum> m;
    volum_->Resource(m);

    std::map<std::string, baidu::galaxy::resource::VolumResource::Volum>::iterator iter = m.begin();

    while (iter != m.end()) {
        boost::shared_ptr<baidu::galaxy::proto::VolumResource> vr(new baidu::galaxy::proto::VolumResource());
        vr->set_medium(iter->second.medium_);
        vr->set_device_path(iter->first);
        baidu::galaxy::proto::Resource* r = new baidu::galaxy::proto::Resource();
        r->set_assigned(iter->second.assigned_);
        r->set_total(iter->second.total_);
        vr->set_allocated_volum(r);
        resource.push_back(vr);
        iter++;
    }
}

void ResourceManager::CalResource(const baidu::galaxy::proto::ContainerDescription& desc,
        int64_t& cpu_millicores,
        int64_t& memroy_require,
        std::vector<const baidu::galaxy::proto::VolumRequired*>& vv)
{

    cpu_millicores = 0L;
    memroy_require = 0L;
    vv.clear();

    for (int i = 0; i < desc.cgroups_size(); i++) {
        memroy_require += desc.cgroups(i).memory().size();
        cpu_millicores += desc.cgroups(i).cpu().milli_core();
    }

    vv.push_back(&desc.workspace_volum());
    for (int i = 0; i < desc.data_volums_size(); i++) {
        if (desc.data_volums(i).medium() == baidu::galaxy::proto::kTmpfs) {
            memroy_require += desc.data_volums(i).size();
            continue;
        }
        vv.push_back(&desc.data_volums(i));
    }

}
}
}
}