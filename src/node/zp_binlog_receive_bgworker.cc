// Copyright 2017 Qihoo
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http:// www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/node/zp_binlog_receive_bgworker.h"

#include <glog/logging.h>
#include <string>
#include <memory>

#include "src/node/zp_data_server.h"
#include "src/node/zp_data_partition.h"

extern ZPDataServer* zp_data_server;

ZPBinlogReceiveBgWorker::ZPBinlogReceiveBgWorker(int full) {
  bg_thread_ = new pink::BGThread(full);
  bg_thread_->set_thread_name("ZPDataSyncWorker");
}

ZPBinlogReceiveBgWorker::~ZPBinlogReceiveBgWorker() {
  bg_thread_->StopThread();
  delete bg_thread_;
  LOG(INFO) << "A ZPBinlogReceiveBgWorker " << bg_thread_->thread_id()
    << " exit!!!";
}

void ZPBinlogReceiveBgWorker::AddTask(ZPBinlogReceiveTask *task) {
  bg_thread_->StartThread();
  bg_thread_->Schedule(&DoBinlogReceiveTask, static_cast<void*>(task));
}

void ZPBinlogReceiveBgWorker::DoBinlogReceiveTask(void* task) {
  ZPBinlogReceiveTask *task_ptr = static_cast<ZPBinlogReceiveTask*>(task);
  PartitionSyncOption option = task_ptr->option;
  std::string table_name = option.table_name;
  uint32_t partition_id = option.partition_id;

  std::shared_ptr<Partition> partition = zp_data_server->GetTablePartitionById(
      table_name, partition_id);
  if (partition == NULL) {
    LOG(WARNING) << "No partition found for BinlogReceiverBgWorker, Partition: "
      << partition_id;
    return;
  }

  switch (option.type) {
    case client::SyncType::CMD:
      partition->DoBinlogCommand(
          option,
          task_ptr->cmd, task_ptr->request);
      break;
    case client::SyncType::SKIP:
      partition->DoBinlogSkip(
          option,
          task_ptr->gap);
      break;
    default:
      LOG(WARNING) << "Unknown binlog sync type: "
        << static_cast<int>(option.type);
  }

  delete task_ptr;
}

