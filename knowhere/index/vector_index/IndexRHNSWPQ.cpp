// Copyright (C) 2019-2020 Zilliz. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance
// with the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied. See the License for the specific language governing permissions and limitations under the License.

#include <algorithm>
#include <cassert>
#include <iterator>
#include <utility>
#include <vector>

#include "common/Exception.h"
#include "common/Log.h"
#include "index/vector_index/IndexRHNSWPQ.h"
#include "index/vector_index/adapter/VectorAdapter.h"
#include "index/vector_index/helpers/FaissIO.h"

namespace knowhere {

IndexRHNSWPQ::IndexRHNSWPQ(int d, int pq_m, int M, MetricType metric) {
    faiss::MetricType mt = GetFaissMetricType(metric);
    index_ = std::shared_ptr<faiss::Index>(new faiss::IndexRHNSWPQ(d, pq_m, M, mt));
}

BinarySet
IndexRHNSWPQ::Serialize(const Config& config) {
    if (!index_) {
        KNOWHERE_THROW_MSG("index not initialize or trained");
    }

    try {
        auto res_set = IndexRHNSW::Serialize(config);
        MemoryIOWriter writer;
        writer.name = QUANTIZATION_DATA;
        auto real_idx = dynamic_cast<faiss::IndexRHNSWPQ*>(index_.get());
        if (real_idx == nullptr) {
            KNOWHERE_THROW_MSG("dynamic_cast<faiss::IndexRHNSWPQ*>(index_) failed during Serialize!");
        }
        faiss::write_index(real_idx->storage, &writer);
        std::shared_ptr<uint8_t[]> data(writer.data_);

        res_set.Append(writer.name, data, writer.rp);
        Disassemble(res_set, config);
        return res_set;
    } catch (std::exception& e) {
        KNOWHERE_THROW_MSG(e.what());
    }
}

void
IndexRHNSWPQ::Load(const BinarySet& index_binary) {
    try {
        Assemble(const_cast<BinarySet&>(index_binary));
        IndexRHNSW::Load(index_binary);
        MemoryIOReader reader;
        reader.name = QUANTIZATION_DATA;
        auto binary = index_binary.GetByName(reader.name);

        reader.total = static_cast<size_t>(binary->size);
        reader.data_ = binary->data.get();

        auto real_idx = dynamic_cast<faiss::IndexRHNSWPQ*>(index_.get());
        if (real_idx == nullptr) {
            KNOWHERE_THROW_MSG("dynamic_cast<faiss::IndexRHNSWPQ*>(index_) failed during Load!");
        }
        real_idx->storage = faiss::read_index(&reader);
        real_idx->init_hnsw();
    } catch (std::exception& e) {
        KNOWHERE_THROW_MSG(e.what());
    }
}

void
IndexRHNSWPQ::Train(const DatasetPtr& dataset_ptr, const Config& config) {
    try {
        GET_TENSOR_DATA_DIM(dataset_ptr)
        faiss::MetricType metric_type = GetFaissMetricType(config);
        int32_t efConstruction = GetIndexParamEfConstruction(config);
        int32_t pqm = GetIndexParamPQM(config);
        int32_t hnsw_m = GetIndexParamHNSWM(config);

        auto idx = new faiss::IndexRHNSWPQ(int(dim), pqm, hnsw_m, metric_type);
        idx->hnsw.efConstruction = efConstruction;
        index_ = std::shared_ptr<faiss::Index>(idx);
        index_->train(rows, reinterpret_cast<const float*>(p_data));
    } catch (std::exception& e) {
        KNOWHERE_THROW_MSG(e.what());
    }
}

int64_t
IndexRHNSWPQ::Size() {
    if (!index_) {
        KNOWHERE_THROW_MSG("index not initialize");
    }
    return dynamic_cast<faiss::IndexRHNSWPQ*>(index_.get())->cal_size();
}

}  // namespace knowhere
