#pragma once

#include "R_MessageStructs.h"

class ExchangeClient
{
  public:
    AddResult addOrder(const AddRequest& request);
    ModifyResult modifyOrder(const ModifyRequest& request);
    CancelResult cancelOrder(const CancelRequest request);
};