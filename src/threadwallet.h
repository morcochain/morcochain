#ifndef THREAD_WALLET_H
#define THREAD_WALLET_H

#include <boost/thread.hpp>

extern boost::thread_specific_ptr<CWallet*> threadWallet;
#define pwalletMain (*threadWallet.get())

#endif
