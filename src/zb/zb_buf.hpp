#ifndef ZB_BUF_HPP_
#define ZB_BUF_HPP_

#include "zb_types.hpp"

namespace zb
{
    struct BufPtr
    {
        ~BufPtr() { if (bufid) zb_buf_free(bufid); }
        zb_bufid_t bufid = 0;
    };
}
#endif

