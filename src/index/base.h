// Copyright (c) 2017-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_INDEX_BASE_H
#define BITCOIN_INDEX_BASE_H

#include <dbwrapper.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <threadinterrupt.h>
#include <uint256.h>
#include <validationinterface.h>

class CBlockIndex;

/**
 * Base class for indices of blockchain data. This implements
 * CValidationInterface and ensures blocks are indexed sequentially according
 * to their position in the active chain.
 */
class BaseIndex : public CValidationInterface
{
protected:
    class DB : public CDBWrapper
    {
    public:
        DB(const fs::path& path, size_t n_cache_size,
           bool f_memory = false, bool f_wipe = false, bool f_obfuscate = false);

        /// Read block locator of the chain that the txindex is in sync with.
        bool ReadBestBlock(CBlockLocator& locator) const;

        /// Write block locator of the chain that the txindex is in sync with.
        void WriteBestBlock(CDBBatch& batch, const CBlockLocator& locator);
    };

private:
    /// Whether the index is in sync with the main chain. The flag is flipped
    /// from false to true once, after which point this starts processing
    /// ValidationInterface notifications to stay in sync.
    std::atomic<bool> m_synced{false};

    /// The last block in the chain that the index is in sync with.
    std::atomic<const CBlockIndex*> m_best_block_index{nullptr};

    std::thread m_thread_sync;
    CThreadInterrupt m_interrupt;

    /// Sync the index with the block index starting from the current best block.
    /// Intended to be run in its own thread, m_thread_sync, and can be
    /// interrupted with m_interrupt. Once the index gets in sync, the m_synced
    /// flag is set and the BlockConnected ValidationInterface callback takes
    /// over and the sync thread exits.
    void ThreadSync();

    /// Load index locator from disk, compare to active chain, and set
    /// m_best_block_index and m_synced.
    bool LoadPosition();

    /// Save index locator to disk, flushing data needed to make the index
    /// consistent, and updating m_best_block_index. Rewind parameter can be set
    /// to true to walk the index back to an ancestor of the previous tip.
    ///
    /// If new position descends from the previous position, it's safe to ignore
    /// errors from this function, because even if new data was partially or
    /// incorrectly written, the old data will still be valid and the previous
    /// position will still be set.
    bool SavePosition(const CBlockIndex* new_tip, bool rewind = false);

    void BlockConnected(const std::shared_ptr<const CBlock>& block, const CBlockIndex* pindex,
                        const std::vector<CTransactionRef>& txn_conflicted) override;

    void ChainStateFlushed(const CBlockLocator& locator) override;

protected:
    /// Initialize internal state before starting sync.
    virtual bool OnStart() { return true; }

    /// Update index entries for a newly connected block.
    virtual bool OnBlock(const CBlock& block, const CBlockIndex* pindex) { return true; }

    /// Rewind index to an earlier chain tip during a chain reorg. The tip will
    /// be an ancestor of the current best block.
    virtual bool OnRewind(CDBBatch& batch, const CBlockIndex* current_tip, const CBlockIndex* new_tip) { return true; }

    /// Save additional state after a set of blocks are added or rewound.
    virtual bool OnFlush(CDBBatch& batch) { return true; }

    virtual DB& GetDB() const = 0;

    /// Get the name of the index for display in logs.
    virtual const char* GetName() const = 0;

public:
    /// Destructor interrupts sync thread if running and blocks until it exits.
    virtual ~BaseIndex();

    /// Blocks the current thread until the index is caught up to the current
    /// state of the block chain. This only blocks if the index has gotten in
    /// sync once and only needs to process blocks in the ValidationInterface
    /// queue. If the index is catching up from far behind, this method does
    /// not block and immediately returns false.
    bool BlockUntilSyncedToCurrentChain();

    void Interrupt();

    /// Start initializes the sync state and registers the instance as a
    /// ValidationInterface so that it stays in sync with blockchain updates.
    void Start();

    /// Stops the instance from staying in sync with blockchain updates.
    void Stop();
};

#endif // BITCOIN_INDEX_BASE_H
