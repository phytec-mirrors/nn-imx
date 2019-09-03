#ifndef __OVX_MEMORY_POOL_H__
#define __OVX_MEMORY_POOL_H__

#include <cstddef>
#include <cstdint>
#include <vector>
#include <list>
#include <map>
#include <tuple>
#include <memory>
// nnapi header
#include "NeuralNetworks.h"

#include "vsi_nn_log.h"

namespace ovxlib{
    //forward declare
    class Memory;
}

namespace mem_pool{
//forward declare
namespace alloc{
    struct Mmp_block_infor;
    struct Nml_block_infor;
    struct Sml_block_infor;
}

/**
 * @brief Memory type indentifier
 *
 */
enum class Type : int{
    MMP = 0,    //!< memory source is memory mapped file
    NML = 1,    //!< memory source is memory pointer with proper size
    SML = 2,    //!< memory source is memory pointer with relative samll size then @NML

    CNT,
    INVALID = CNT,
};

/**
 * @brief reference object as unique descriptor for memory block
 * client SHOULD not create instance of Ref but keep a shared_ref
 * object to memory pool managed by memory-pool
 *
 * @code{.cpp}
 * shared_ref reference = mem_pool::global_memory_pool()::add_reference(...);
 * if (reference) {
 *  // Do somthing with your shared_ptr
 * }
 * @endcode
 *
 */
struct Ref {
    // upper-part: user
    const void* address_;     //!< real memory address for read
    size_t len_;              //!< memory block size in bytes

    // lower-part: internal purpose
    Type mem_type_;     //!< memory type @see Type
    uint32_t index_;    //!< used when release reference
    uint32_t offset_;   //!< used when release reference
    const void* src_;   //!< memory address or mmap obj
};

typedef std::shared_ptr<Ref> shared_ref;

typedef ovxlib::Memory MmapSegment; // TODO, remove this once renamed ovxlib::Memory

/**
 * @brief Data struct managed memory mapped information
 * Memory mapped file may referenced by different operand with different offset and
 * length, if more than two reference created on same memory mapped file, we need
 * maintain a count for it.
 */
struct Mmp_block_list{
    typedef std::map<const MmapSegment*, unsigned int> mem_list_type;
    mem_list_type mem_list_;   //!< second is a shared count

    /**
     * @brief Add reference to given memory mapped block
     *
     * @param infor Which memory mapped space referenced, and offset + length in bytes
     * @param mem_ref if offset + length < upper bound of memory mapped space, set
     *        mem_ref.len_ as requested in infor, or mem_ref.len_ setup by
     *        offset + length - sizeof(memory mapped space)
     * @return true offset not exceed upper bound of memory mapped space
     * @return false offset exceed upper bound of memory mapped space
     */
    bool add_reference(const alloc::Mmp_block_infor& infor, Ref& mem_ref);

    /**
     * @brief delete given memory reference
     *
     * @param mem_ref content of memory reference
     */
    void del_reference(const Ref& mem_ref);
};

/**
 * @brief Data struct manage normal block information
 *
 *  interface description @see Mmp_block_list
 */
struct Nml_block_list{
     /**
     * @brief Read Only Segment descriptor
     */
    struct Ro_segment{
        const void* address_;   //!< source memory address referenced by client
        size_t len_;            //!< size in bytes
    };

    typedef std::map<Ro_segment, int> mem_list_type;
    mem_list_type mem_list_;

    bool add_reference(const alloc::Nml_block_infor& infor, Ref& mem_ref);
    void del_reference(const Ref& mem_ref);
};
/**
 * @brief Data struct manage small blocks, it will allocate/free memory space internally
 *
 */
struct Sml_block_list{
    static const size_t small_block_shreshold = ANEURALNETWORKS_MAX_SIZE_OF_IMMEDIATELY_COPIED_VALUES;

    std::vector<std::vector<uint8_t>> mem_list_;
    std::map<Ref, int> ref_count_; //!<

    Sml_block_list();

    bool add_reference(const alloc::Sml_block_infor& infor, Ref& mem_ref);
    void del_reference(const Ref& mem_ref);
};

/**
 * @brief manage different type of memory references
 * Provide easy-to-used interface to add/delete memory reference
 *
 */
struct Manager{
    Manager() {}

    shared_ref add_reference(const void* address, size_t len, bool is_output = false);

    shared_ref add_reference(const ovxlib::Memory* address, size_t offset, size_t len);

    void del_reference(const Ref& mem_ref) ;

private:
    /**
     * @brief Template for add memory reference
     * return shared_ptr of Ref object. Use it as raw-pointer,
     * client should check it not a null_ptr before access it.
     *
     * @tparam BlockAllocInfor memory "allocate" type
     * @param infor
     * @return shared_ref   if add reference failed, return null_ptr
     */
    template <typename BlockAllocInfor>
    shared_ref add_reference(const BlockAllocInfor& infor) ;

    std::tuple<Mmp_block_list, Nml_block_list, Sml_block_list> space_;
};

Manager& global_memory_pool();
}
#endif
