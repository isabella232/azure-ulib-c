// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef AZ_ULIB_USTREAM_BASE_H
#define AZ_ULIB_USTREAM_BASE_H

/**
 * @file az_ulib_ustream_base.h
 * @brief **uStream Interface Definition**
 *
 *  This is the definition of a heterogeneous buffer that helps other modules in a system
 *      expose large amounts of data without using a large amount of memory. Modules in the system can expose
 *      their own data using this interface. To do that, the module shall implement the functions in the
 *      interface. This implementation shall follow the definition described in this file, which includes
 *      not only the prototype of the header, but the behavior as well.
 *  uStream defines a provider-consumer interface when:
 *      - <b>Provider</b> - is the module of code that handles data to be exposed. This module implements
 *          the ustream interface to expose the data to the consumer.
 *      - <b>Consumer</b> - is the module of code that will use the data exposed by the provider.
 *
 *  The ustream shall have a clear separation between the internal content (provider domain)
 *      and what it exposes as external content (consumer domain). The ustream shall never expose 
 *      the internal content (ex: providing a pointer to a internal memory position). All 
 *      exposed content shall be copied from the internal data source to some given external memory. To do
 *      that in a clear way, the ustream shall always work with the concept of two buffers, the 
 *      <tt>data source</tt> and the <tt>local buffer</tt>, adhering to the following definition:
 *      - <b>Data source</b> - is the place where the data is stored by the implementation of the ustream
 *          interface. The data source is in the provider domain, and it shall be protected, immutable, 
 *          and non volatile. Consumers can read the data from the data source by the calling
 *          the az_ulib_ustream_read() API, which will copy a snapshot of the data to the
 *          provided external memory, called local buffer.
 *      - <b>Local buffer</b> - is the consumer domain buffer, where the az_ulib_ustream_read() API 
 *          will copy the required bytes from the data source. The local buffer belongs to the consumer 
 *          of this interface, which means that the consumer shall allocate and free (if necessary) this memory,
 *          and the content of the local buffer can be changed and released.
 * 
 * <i><b>Example</b></i>
 *
 *  A provider wants to create a ustream to expose data to the consumer. The provider will 
 *      store the content in the HEAP, and will create a ustream from it, passing the ownership of 
 *      the content to the ustream. Consumer will print the content of the ustream, using a 
 *      local buffer of 1K. The following diagram represents this operation.
 *
 * @code
 *  +----------------+        +----------------+              +------------------+     +------------+
 *  |    Provider    |        |    Consumer    |              |      ustream     |     |    HEAP    |
 *  +----------------+        +----------------+              +------------------+     +------------+
 *          |           +-------------+                                  |                    |
 *          |           | [Allocate on stack or heap]                    |                    |
 *          |           | [In this example the stack]                    |                    |
 *          |           | az_ulib_ustream ustream_instance               |                    |
 *          |           +-------------+                                  |                    |
 *          |                         |                                  |                    |
 *          |<-get_provider_content                                      |                    |
 *               (&ustream_instance)--+                                  |                    |
 *          +----------------------------malloc(content_size)-------------------------------->|
 *          |<--------------------------------content_ptr-------------------------------------+
 *          +--------------------malloc(sizeof(az_ulib_ustream_data_cb))--------------------->|
 *          <---------------------------------control_block_ptr-------------------------------+
 *   +------+                         |                                  |                    |
 *   | generate the content and store in the content_ptr                 |                    |
 *   +----->|                         |                                  |                    |
 *          +-----az_ulib_ustream_init                                   |                    |
 *          |       (ustream_instance,                                   |                    |
 *          |        control_block_ptr, free,                            |                    |
 *          |        content_ptr, content_size, free)------------------->|                    |
 *          |                         |                           +------+                    |
 *          |                         |                           | data_source = content_ptr |
 *          |                         |                           | data_source_size = content_size
 *          |                         |                           +----->|                    |
 *          |<-----------------ustream_instance--------------------------+                    |
 *          +----AZ_ULIB_SUCCESS----->|                                  |                    |
 * @endcode
 *
 *
 *  Now that the consumer has it's local ustream intialized with the content, it will print it using the 
 *   iterator az_ulib_ustream_read().
 *
 * @code
 *          |                         +------------------malloc(1024)------------------------>|
 *          |                         |<-----------------local_buffer-------------------------+
 *  .. while az_ulib_ustream_read return AZ_ULIB_SUCCESS ...............................................
 *  :       |                         +--az_ulib_ustream_read            |                    |         :
 *  :       |                         |  (ustream_instance,              |                    |         :
 *  :       |                         |   local_buffer,                  |                    |         :
 *  :       |                         |   1024,                          |                    |         :
 *  :       |                         |   &size)------------------------>|                    |         :
 *  :       |                         |                 +----------------+                    |         :
 *  :       |                         |                 | copy the next 1024 bytes from the   |         :
 *  :       |                         |                 |  data_source to the local_buffer.   |         :
 *  :       |                         |                 +--------------->|                    |         :
 *  :       |                         |<---AZ_ULIB_SUCCESS---------------+                    |         :
 *  :       |                     +---+                                  |                    |         :
 *  :       |                     | use the content in the local_buffer  |                    |         :
 *  :       |                     +-->|                                  |                    |         :
 *  .....................................................................................................
 *          |                         +---------------free(local_buffer)--------------------->|
 *          |                         +-az_ulib_ustream_dispose          |                    |
 *          |                         |       (ustream_instance)-------->|                    |
 *          |                         |                            +-----+                    |
 *          |                         |                            | free(control_block_ptr)->|
 *          |                         |                            | free(data_source)------->|
 *          |                         |                            +-----+                    |
 * @endcode
 *
 * <h2>Heterogeneous buffer</h2>
 *  Data can be stored in multiple, different medias, like RAM, flash, file, or cloud. Each media
 *      has its own read requirements. A simple way to unify it is copying it all to the RAM. For
 *      example, if an HTTP package contains a header that is in the flash, with some data in the RAM
 *      and the content in a file in the external flash, to concatenate it all in a single datagram
 *      you can allocate a single area in the RAM that fits it all, and bring all the data to this
 *      memory. The problem with this approach is the amount of memory required for that, which can be
 *      multiple times the total RAM that you have for the entire system.
 *
 *  A second option to solve this problem is to make each component that needs to access this data
 *      understand each media and implement code to handle it. This approach will not require
 *      storing all data in the RAM, but will increase the size of the program itself, and
 *      is not easily portable, as different hardware will contain different media with different
 *      requirements.
 *
 *  The ustream solves this problem by creating a single interface that can handle any media,
 *      exposing it as a standard iterator. Whoever wants to expose a type of media as a ustream shall
 *      implement the functions described on the interface, handling all implementation details for
 *      each API. For example, the az_ulib_ustream_read() can be a simple copy of the flash to
 *      the RAM for a buffer that handles constants, or be as complex as creating a TCP/IP connection
 *      to bring the data for a buffer that handles data in the cloud.
 *
 *  The consumer of the ustream can use all kinds of media in the same way, and may easily
 *      concatenate it by exposing a ustream that handles multiple ustream's.
 * 
 * <h2>Static Memory</h2>
 *  The ustream does not use any calls to <tt>malloc</tt>. Memory that it uses is required to be passed by the developer
 *      with an associated release function unless otherwise specified.
 *
 * <h2>Ownership</h2>
 *  The ustream is an owner-less buffer: every instance of the ustream has the same rights. They
 *      all can read the ustream content, release the parts that are not necessary anymore, and dispose
 *      it.
 *  Each instance of the ustream is owned by who created it, and should never be shared by 
 *      multiple consumers. When a consumer receives a ustream and intends to make operations over 
 *      it, this consumer must first make a clone of the ustream, creating its own instance of it, and 
 *      then make the needed operations.
 * Cloning a ustream creates a new set of controls for the ustream that will share the same content of
 *      the original ustream. The content itself is a smart pointer with a <tt>ref_count</tt> that
 *      controls the total number of instances.
 * Disposing an instance of the ustream will decrease the <tt>ref_count</tt> of this ustream. If the
 *      number of references reaches 0, the ustream will destroy itself by calling the provided release functions.
 *
 *  @warning Not disposing an instance of the ustream will leak memory.
 *
 *  Instances of the ustream can be created in 2 ways:
 *      - @b Factory - when a producer exposes data using a ustream, it must create the ustream
 *          using a factory, so the operation <tt>ustream create</tt> returns the first instance of the
 *          ustream.
 *      - @b Clone - when a consumer needs a copy of the ustream, it can use the az_ulib_ustream_clone().
 *
 * <h2>Thread Safe</h2>
 *  The ustream <b>IS NOT</b> thread safe for multiple accesses over the same instance. The ownership
 *      of the instance of a ustream shall <b>NOT</b> be shared, especially not by consumers that run on 
 *      different threads. The owner thread shall create a clone of the ustream and pass it to the other 
 *      thread.
 *  The ustream <b>IS</b> thread safe for accesses between instances. It means that any access to
 *      memory shared by multiple instances shall be thread safe.
 *
 * <h2>Data Retention</h2>
 *  As with any buffer, this ustream shall be used to handle data that was created by the producer as a
 *      result of an operation.
 *
 *  This interface only exposes read functions, so once created, the content of the ustream cannot
 *      be changed by the producer or any of the consumers. Changing the content of the data source will
 *      result in a data mismatch.
 *
 *  Consumers can do a partial release of the ustream by calling az_ulib_ustream_release().
 *      Calling the release does not imply that part of the memory will be immediately released. Once a
 *      ustream can handle multiple instances, a memory can only be free'd if all instances release it.
 *      A ustream implementation can or cannot have the ability to do partial releases. For instance, a
 *      ustream that handles constant data stored in the flash will never release any memory on the
 *      az_ulib_ustream_release() API.
 *
 *  Released data cannot be accessed, even if it is still available in the memory.
 *
 * <h2>Concatenate</h2>
 *  New data can be concatenated at the end of the ustream by calling az_ulib_ustream_concat().
 *      This can include ustream's from other different medias. In this way, the ustream can
 *      be used as a Stream of data.
 *  To protect the immutability of the ustream, concatenating a new ustream to an existing one will
 *      only affect the instance that is calling the az_ulib_ustream_concat().
 *
 * <i><b>Example</b></i>
 *  A producer created 3 ustreams named A, B, and C. At this point, it handles one instance of each
 *      ustream. A consumer received an instance of the ustream A and C, and concats C to A creating a new ustream AC.
 *      After that, the producer will concat B to A, creating the new AB ustream.
 *
 *  Observe the fact that the consumer concatenating C to A on its own instance didn't affect the ustream
 *      A on the producer, and when the producer concatenated B to A, it creates AB, not ACB, and it didn't change
 *      the consumer AB ustream creating ABC or ACB on it.
 *
 * <h2>Lazy</h2>
 *  The ustream can contain the full content, bring it into memory when required, or even create the content
 *      when it is necessary. The implementation of the az_ulib_ustream_read() function can be smart 
 *      enough to use the minimal amount of memory.
 *
 *  The only restriction is if a consumer accesses the same position of the ustream multiple times, it shall
 *      return the same data.
 *
 * <i><b>Example</b></i>
 *  A random number generator can expose random numbers using the ustream. To do that it shall generate a 
 *      new number when the consumer calls az_ulib_ustream_read(). But to preserve the immutability,
 *      the implementation of the az_ulib_ustream_read() shall store the number in a recover queue, up
 *      to the point that the consumer releases this data. Because, if at some point in time, the consumer 
 *      seeks this old position, the az_ulib_ustream_read() shall return the same value created in
 *      the first call of az_ulib_ustream_read().
 *
 * <h2>Data conversion</h2>
 *  When the data is copied from the data source to the local buffer, the az_ulib_ustream_read()
 *      may do a data conversion, which means that the content exposed on the local buffer is a function
 *      of the content in the data source. It directly implies that the number of bytes written in the
 *      local buffer may be different than the number of bytes read from the data source.
 *
 * <i><b>Example</b></i>
 *  A ustream can have the data source in binary format with 36 bytes, but it shall expose the 
 *      content encoded in base64. The base64 creates 4 encoded bytes for each 3 bytes read. So, seeking the 
 *      beginning of the file, the az_ulib_ustream_get_remaining_size() shall return 48 (= 36 / 3 * 4),
 *      instead of 36. If the consumer provides a local buffer of 16 bytes, the az_ulib_ustream_read() 
 *      shall read only 12 bytes from the data source, and encode it in base64 expanding the 12 bytes to 
 *      16 bytes on the local buffer.
 * @code
 *                  ustream domain                      ::      consumer domain
 *                                                      ::
 *                    Data source                       ::
 *                    +-------+--------------------+    ::
 *    binary data --> |       |                    |    ::
 *                    +-------+--------------------+    ::
 * inner position --> 0       12                   36   ::
 *                     \--+--/                          ::      Local buffer
 *                        | size = 12                   ::      +----------------+
 *                        +---> base64 encoder ---------------> | base64         |
 *                                                      ::      +----------------+
 *                                                      ::        size' = 16
 * @endcode
 *
 * <h2>Data offset</h2>
 *  In the data source, each byte is associated with a position, called <tt>inner position</tt>. The first 
 *      byte is always placed at the inner position <tt>0</tt>, followed by the other bytes which are incremented
 *      in a sequential manner. The ustream assigns a sequential number to each byte
 *      in the local buffer as well, called <tt>logical position</tt>. When a new ustream is created,
 *      the logical position matches the inner position, both starting at position <tt>0</tt>.
 *
 *  When the ustream is cloned, an offset shall be provided. This offset is the new first logical 
 *      position. The implementation of the ustream shall handle the difference between the inner
 *      and logical position, making the conversion in all the ustream API. Providing an offset to a 
 *      ustream can be useful in many cases. For example, to concatenate buffers, the second ustream can 
 *      have an offset of the end of the first ustream plus one, or in a TCP connection, make the 
 *      logical position the same value of the octet sequence number.
 *
 * <i><b>Example</b></i>
 *  A ustream was created from the flash with 100 bytes. The inner position is a sequence from
 *      <tt>0</tt> to <tt>99</tt>, and it matches the logical position. The consumer clones this ustream providing an
 *      offset of <tt>1000</tt>. The new instance contains the same content as the original one, but the 
 *      logical positions are now from <tt>1000</tt> to <tt>1099</tt>.
 *
 *  If the owner of the first instance wants to set the position to position 10, it shall call az_ulib_ustream_set_position()
 *      with the logical position 10. For the cloned instance, to set the position to the same position 10, it shall call 
 *      az_ulib_ustream_set_position() with the logical position 1010.
 *
 * <h2>Sliding window</h2>
 *  One of the target use cases of the ustream is to accelerate and simplify the implementation of 
 *      sliding window protocols, like TCP. As described in this document, the ustream associates 
 *      a single byte (octet) to a single position, which means that every byte can be accessed by its
 *      position. For the consumer, this position is the logical position.
 *
 *  To better understand the sliding window concept of the ustream, the Data source can be split
 *      in 4 segments.
 *
 * @code
 *      Data Source:
 *           Released                       Pending                          Future
 *       |----------------|---------------------:--------------------|---------------------|
 *       |\                \                    :       Read         |\                    |
 *       | 0           First Valid Position     :                    | Current Position    |
 *       |                                      :                    |                     |
 *       |                                      :<--- Read Size ---->|                     |
 *       |                                                                                 |
 *       |<------------------------------ Data Source Size ------------------------------->|
 * @endcode
 *      - @b Released - Sequence of bytes in the data source that is already acknowledged by the consumer, 
 *          and shall not be accessed anymore.
 *      - @b Pending - Sequence of bytes in the data source that is already read by the consumer, but not 
 *          acknowledged yet. The consumer can seek these bytes with az_ulib_ustream_set_position() and read it again. 
 *          This sequence starts at the <tt>First Valid Position</tt> and ends at the last byte before the <tt>Current Position</tt>.
 *      - @b Read - The last read portion of the data source. On the read operation, the <tt>Read</tt> starts
 *          at the <tt>Current Position</tt> up to the <tt>Read Size</tt>. At the end of the read, this segment is 
 *          incorporated to <tt>Pending</tt> by changing the <tt>Current Position</tt> to the end of the Read.
 *      - @b Future - Sequence of bytes in the data source that is not read by the consumer yet. It starts 
 *          at the <tt>Current Position</tt> and ends at the end of the data source, which has the position calculated
 *          by <tt>Data Source Size - 1</tt>. 
 *
 *  To read a new portion of the data source, the consumer shall provide memory (the local buffer), where 
 *      the implementation of the ustream will write the bytes that were read and converted from the data source. 
 *      The consumer can use this data in its own context: for example, to transmit as a TCP packet. When
 *      the consumer finishes using the data in the local buffer, this data can be discarded
 *      and the local buffer recycled to get the next portion of the data source.
 *
 *  If at some point in the future, the consumer needs this data again, it can set the position to the needed position 
 *      and get the same content using the read.
 *
 *  The consumer may confirm that a portion of the data is not necessary anymore. For example, after transmitting
 *      multiple TCP packets, the receiver of these packets answers with an ACK for a sequence number. In this case,
 *      the consumer can release this data in the data source by calling the az_ulib_ustream_release(), moving 
 *      the <tt>First Valid Position</tt> to the next one after the released position.
 *
 *  A common scenario is when the consumer needs to read over the data source starting on the first byte after
 *      the last released one. For example, when a timeout happens for a transmitted packet without ACK, the 
 *      sender shall retransmit the data starting from that point. In this case, the consumer can call the API
 *      az_ulib_ustream_reset().
 */

#include "az_ulib_config.h"
#include "az_ulib_result.h"
#include "az_ulib_pal_os.h"

#ifdef __cplusplus
#include <cstdint>
#include <cstddef>
extern "C" {
#else
#include <stdint.h>
#include <stddef.h>
#endif /* __cplusplus */

/**
 * @brief   Define offset_t with the same size as size_t.
 */
typedef size_t offset_t;

/**
 * @brief   Forward declaration of az_ulib_ustream. See #az_ulib_ustream_tag for struct members.
 */
typedef struct az_ulib_ustream_tag az_ulib_ustream;

/**
 * @brief   vTable with the ustream APIs.
 *
 *  Any module that exposes the ustream shall implement the functions on this vTable.
 *
 *  Any code that will use an exposed ustream shall call the APIs using the <tt>az_ulib_ustream_...</tt>
 *      inline functions.
 */
typedef struct az_ulib_ustream_interface_tag
{
    az_ulib_result(*set_position)(az_ulib_ustream* ustream_instance, offset_t position);         /**<concrete <tt>set_position</tt> implementation*/
    az_ulib_result(*reset)(az_ulib_ustream* ustream_instance);                                   /**<concrete <tt>reset</tt> implementation*/
    az_ulib_result(*read)(az_ulib_ustream* ustream_instance, uint8_t* const buffer, 
                                            size_t buffer_length, size_t* const size);           /**<concrete <tt>read</tt> implementation*/
    az_ulib_result(*get_remaining_size)(az_ulib_ustream* ustream_instance, size_t* const size);  /**<concrete <tt>get_remaining_size</tt> implementation*/
    az_ulib_result(*get_position)(az_ulib_ustream* ustream_instance, offset_t* const position);  /**<concrete <tt>get_position</tt> implementation*/
    az_ulib_result(*release)(az_ulib_ustream* ustream_instance, offset_t position);              /**<concrete <tt>release</tt> implementation*/
    az_ulib_result(*clone)(az_ulib_ustream* ustream_instance_clone, 
                                            az_ulib_ustream* ustream_instance, offset_t offset); /**<concrete <tt>clone</tt> implementation*/
    az_ulib_result(*dispose)(az_ulib_ustream* ustream_instance);                                 /**<concrete <tt>dispose</tt> implementation*/
} az_ulib_ustream_interface;

/**
 * @brief   Signature of the function to release memory passed to the ustream
 * 
 * @param[in]   release_pointer       void pointer to memory that needs to be free'd
 * 
 */
typedef void (*az_ulib_release_callback)(void* release_pointer);

/**
 * @brief   Pointer to the data from which to read
 * 
 * void pointer to memory where the data is located or any needed controls to access the data.
 * The content of the memory to which this points is up to the ustream implementation.
 * 
 */
typedef void* az_ulib_ustream_data;

/**
 * @brief   Structure for data control block
 * 
 * For any given ustream that is created, one control block is created and initialized.
 * 
 * @note This structure should be viewed and used as internal to the implementation of the ustream. Users should therefore not act on
 *       it directly and only allocate the memory necessary for it to be passed to the ustream.
 * 
 */
typedef struct az_ulib_ustream_data_cb_tag
{
    const az_ulib_ustream_interface* api;       /**<The #az_ulib_ustream_interface*
                                                            for this ustream instance type */
    az_ulib_ustream_data* ptr;                  /**<The #az_ulib_ustream_data* pointing to the data to read. It can 
                                                            be anything that a given ustream implementation needs to
                                                            access the data, whether it be a memory address to a buffer,
                                                            another struct with more controls, etc */
    volatile uint32_t ref_count;                /**<The <tt>volatile uint32_t</tt> with the number of references
                                                            taken for this memory */
    az_ulib_release_callback data_release;           /**<The #az_ulib_release_callback to call to release <tt>ptr</tt> 
                                                            once the <tt>ref_count</tt> goes to zero */
    az_ulib_release_callback control_block_release;   /**<The #az_ulib_release_callback to call to release the #az_ulib_ustream_data_cb
                                                            once the <tt>ref_count</tt> goes to zero */
} az_ulib_ustream_data_cb;

/**
 * @brief   Structure for instance control block
 * 
 * For any given ustream that is created, there may be mutliple <tt>az_ulib_ustream</tt>'s
 *      pointing to the same <tt>az_ulib_ustream_data_cb</tt>. Each instance control block serves to
 *      manage a given developer's usage of the memory pointed to inside the <tt>az_ulib_ustream_data_cb</tt>.
 *      Each time an <tt>az_ulib_ustream</tt> is cloned using az_ulib_ustream_clone(), the
 *      <tt>ref_count</tt> inside the <tt>az_ulib_ustream_data_cb</tt> is incremented to signal a reference
 *      to the memory has been acquired. Once the instance is done being used, az_ulib_ustream_release() 
 *      must be called to decrement <tt>ref_count</tt>.
 * 
 * @note This structure should be viewed and used as internal to the implementation of the ustream. Users should therefore not act on 
 *       it directly and only allocate the memory necessary for it to be passed to the ustream.
 */
struct az_ulib_ustream_tag
{
    /* Control Block */
    az_ulib_ustream_data_cb* control_block;     /**<The #az_ulib_ustream_data_cb* on which this instance operates on. */

    /* Instance controls */
    offset_t offset_diff;                       /**<The #offset_t used as the logical position for this instance. */
    offset_t inner_current_position;            /**<The #offset_t used to keep track of the current position (next returned position). */
    offset_t inner_first_valid_position;        /**<The #offset_t used to keep track of the earliest position to reset. */
    size_t length;                              /**<The <tt>size_t</tt> with the length of the data in the control_block */
};

/**
 * @brief   Structure to keep track of concatenated ustreams.
 * 
 * When concatenating a ustream to another ustream, the instances are placed into a <tt>az_ulib_ustream_multi_data_cb</tt>. The base ustream onto which you wish
 *      to concatenate will be copied into the <tt>ustream_one</tt> structure and the ustream to concatenate will be cloned into the <tt>ustream_two</tt>
 *      structure. The difference being that the first #az_ulib_ustream*, when returned, will point to the newly populated multi instance and the ownership
 *      of the passed instance will be assumed by the multi instance. The second ustream which was passed will not be changed, only cloned into the
 *      <tt>az_ulib_ustream_multi_data_cb</tt> structure.
 * 
 * @note This structure should be viewed and used as internal to the implementation of the ustream. Users should therefore not act on 
 *       it directly and only allocate the memory necessary for it to be passed to the ustream.
 * 
 */
typedef struct az_ulib_ustream_multi_data_cb_tag
{
    az_ulib_ustream_data_cb control_block;          /**<The #az_ulib_ustream_data_cb to manage the multi data structure*/
    az_ulib_ustream ustream_one;                    /**<The #az_ulib_ustream with the first ustream instance*/
    az_ulib_ustream ustream_two;                    /**<The #az_ulib_ustream with the second ustream instance*/
    volatile uint32_t ustream_one_ref_count;        /**<The <tt>uint32_t</tt> with the number of references to the first ustream */
    volatile uint32_t ustream_two_ref_count;        /**<The <tt>uint32_t</tt> with the number of references to the second ustream */
    az_ulib_pal_os_lock lock;                       /**<The #az_ulib_pal_os_lock with controls the critical section of the read from the multi ustream */
} az_ulib_ustream_multi_data_cb;

/**
 * @brief   Check if a handle is the same type of the API.
 *
 *  It will return true if the handle is valid and it is the same type of the API. It will
 *      return false if the handle is <tt>NULL</tt> or not the correct type.
 */
#define AZ_ULIB_USTREAM_IS_NOT_TYPE_OF(handle, type_api)   ((handle == NULL) || (handle->control_block == NULL) || (handle->control_block->api == NULL) || (handle->control_block->api != &type_api))

/**
 * @brief   Change the current position of the ustream.
 *
 *  The current position is the one that will be returned in the local buffer by the next
 *      az_ulib_ustream_read(). Consumers can call this API to go back or forward, but it cannot exceed
 *      the end of the ustream or precede the fist valid position (last released position + 1).
 *
 *  The <tt>az_ulib_ustream_set_position</tt> API shall follow these minimum requirements:
 *      - The <tt>set_position</tt> shall change the current position of the ustream.
 *      - If the provided position is out of the range of the ustream, the <tt>set_position</tt> shall return
 *          #AZ_ULIB_NO_SUCH_ELEMENT_ERROR, and will not change the current position.
 *      - If the provided position is already released, the <tt>set_position</tt> shall return
 *          #AZ_ULIB_NO_SUCH_ELEMENT_ERROR, and will not change the current position.
 *      - If the provided interface is <tt>NULL</tt>, the <tt>set_position</tt> shall return #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR.
 *      - If the provided interface is not the implemented ustream type, the <tt>set_position</tt> shall return
 *          #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR.
 *
 * @param[in]   ustream_instance        The #az_ulib_ustream* with the interface of the ustream. It
 *                                      cannot be <tt>NULL</tt>, and it shall be a valid ustream that is the
 *                                      implemented ustream type.
 * @param[in]   position                The <tt>offset_t</tt> with the new current position in the ustream.
 *
 * @return The #az_ulib_result with the result of the <tt>set_position</tt> operation.
 *          @retval     #AZ_ULIB_SUCCESS                If the ustream changed the current position with success.
 *          @retval     #AZ_ULIB_BUSY_ERROR             If the resource necessary for the <tt>set_position</tt> operation is busy.
 *          @retval     #AZ_ULIB_CANCELLED_ERROR        If the <tt>set_position</tt> operation was cancelled.
 *          @retval     #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR If one of the provided parameters is invalid.
 *          @retval     #AZ_ULIB_NO_SUCH_ELEMENT_ERROR  If the position is out of the ustream range.
 *          @retval     #AZ_ULIB_OUT_OF_MEMORY_ERROR    If there is not enough memory to execute the
 *                                                        <tt>set_position</tt> operation.
 *          @retval     #AZ_ULIB_SECURITY_ERROR         If the <tt>set_position</tt> operation was denied for security
 *                                                        reasons.
 *          @retval     #AZ_ULIB_SYSTEM_ERROR           If the <tt>set_position</tt> operation failed on the system level.
 */
static inline az_ulib_result az_ulib_ustream_set_position(az_ulib_ustream* ustream_instance, offset_t position)
{
    return ustream_instance->control_block->api->set_position(ustream_instance, position);
}

/**
 * @brief   Changes the current position to the first valid position.
 *
 *  The current position is the one that will be returned in the local buffer by the next
 *      az_ulib_ustream_read(). Reset will bring the current position to the first valid one, which
 *      is the first byte after the released position.
 *
 *  The <tt>az_ulib_ustream_reset</tt> API shall follow the following minimum requirements:
 *      - The <tt>reset</tt> shall change the current position of the ustream to the first byte after the
 *          released position.
 *      - If all bytes are already released, the ustream <tt>reset</tt> shall return
 *          #AZ_ULIB_NO_SUCH_ELEMENT_ERROR, and will not change the current position.
 *      - If the provided interface is <tt>NULL</tt>, the ustream <tt>reset</tt> shall return #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR.
 *      - If the provided interface is not the implemented ustream type, the ustream <tt>reset</tt> shall return
 *          #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR.
 *
 * @param[in]   ustream_instance        The #az_ulib_ustream* with the interface of the ustream. It
 *                                      cannot be <tt>NULL</tt>, and it shall be a valid ustream that is the
 *                                      implemented ustream type.
 *
 * @return The #az_ulib_result with the result of the <tt>reset</tt> operation.
 *          @retval     #AZ_ULIB_SUCCESS                If the ustream changed the current position with success.
 *          @retval     #AZ_ULIB_BUSY_ERROR             If the resource necessary for the <tt>reset</tt> operation is
 *                                                        busy.
 *          @retval     #AZ_ULIB_CANCELLED_ERROR        If the <tt>reset</tt> operation was cancelled.
 *          @retval     #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR If one of the provided parameters is invalid.
 *          @retval     #AZ_ULIB_NO_SUCH_ELEMENT_ERROR  If all previous bytes in the ustream were already
 *                                                        released.
 *          @retval     #AZ_ULIB_OUT_OF_MEMORY_ERROR    If there is not enough memory to execute the
 *                                                        <tt>reset</tt> operation.
 *          @retval     #AZ_ULIB_SECURITY_ERROR         If the <tt>reset</tt> operation was denied for security
 *                                                        reasons.
 *          @retval     #AZ_ULIB_SYSTEM_ERROR           If the <tt>reset</tt> operation failed on the system level.
 */
static inline az_ulib_result az_ulib_ustream_reset(az_ulib_ustream* ustream_instance)
{
    return ustream_instance->control_block->api->reset(ustream_instance);
}

/**
 * @brief   Gets the next portion of the ustream starting at the current position.
 *
 * The <tt>az_ulib_ustream_read</tt> API will copy the contents of the Data source to the local buffer
 *      starting at the current position. The local buffer is the one referenced by the parameter
 *      <tt>buffer</tt>, and with the maximum size <tt>buffer_length</tt>.
 *
 *  The buffer is defined as a <tt>uint8_t*</tt> and can represent any sequence of data. Pay
 *      special attention, if the data is a string, the buffer will still copy it as a sequence of
 *      <tt>uint8_t</tt>, and will <b>NOT</b> put any terminator at the end of the string. The size of 
 *      the content copied in the local buffer will be returned in the parameter <tt>size</tt>.
 *
 *  The <tt>az_ulib_ustream_read</tt> API shall follow the following minimum requirements:
 *      - The read shall copy the contents of the <tt>Data Source</tt> to the provided local buffer.
 *      - If the contents of the <tt>Data Source</tt> is bigger than the <tt>buffer_length</tt>, the read shall
 *          limit the copy size up to the buffer_length.
 *      - The read shall return the number of valid <tt>uint8_t</tt> values in the local buffer in 
 *          the provided <tt>size</tt>.
 *      - If there is no more content to return, the read shall return
 *          #AZ_ULIB_EOF, size shall be set to 0, and will not change the contents
 *          of the local buffer.
 *      - If the provided buffer_length is zero, the read shall return #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR.
 *      - If the provided buffer_length is lower than the minimum number of bytes that the ustream can copy, the 
 *          read shall return #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR.
 *      - If the provided interface is <tt>NULL</tt>, the read shall return #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR.
 *      - If the provided interface is not the implemented ustream type, the read shall return
 *          #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR.
 *      - If the provided local buffer is <tt>NULL</tt>, the read shall return #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR.
 *      - If the provided return size pointer is <tt>NULL</tt>, the read shall return
 *          #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR and will not change the local buffer contents or the
 *          current position of the buffer.
 *
 * @param[in]       ustream_instance        The #az_ulib_ustream* with the interface of the ustream. It
 *                                          cannot be <tt>NULL</tt>, and it shall be a valid ustream that is the
 *                                          implemented ustream type.
 * @param[out]      buffer                  The <tt>uint8_t* const</tt> that points to the local buffer. It cannot be <tt>NULL</tt>.
 * @param[in]       buffer_length           The <tt>size_t</tt> with the size of the local buffer. It shall be
 *                                          bigger than 0.
 * @param[out]      size                    The <tt>size_t* const</tt> that points to the place where the read shall store
 *                                          the number of valid <tt>uint8_t</tt> values returned in the local buffer. It cannot be <tt>NULL</tt>.
 *
 * @return The #az_ulib_result with the result of the read operation.
 *          @retval     #AZ_ULIB_SUCCESS                If the ustream copied the content of the <tt>Data Source</tt> to the local buffer
 *                                                        with success.
 *          @retval     #AZ_ULIB_BUSY_ERROR             If the resource necessary to read the ustream content is busy.
 *          @retval     #AZ_ULIB_CANCELLED_ERROR        If the read of the content was cancelled.
 *          @retval     #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR If one of the provided parameters is invalid.
 *          @retval     #AZ_ULIB_EOF                    If there are no more <tt>uint8_t</tt> values in the <tt>Data Source</tt> to read.
 *          @retval     #AZ_ULIB_OUT_OF_MEMORY_ERROR    If there is not enough memory to execute the read.
 *          @retval     #AZ_ULIB_SECURITY_ERROR         If the read was denied for security reasons.
 *          @retval     #AZ_ULIB_SYSTEM_ERROR           If the read operation failed on the system level.
 */
static inline az_ulib_result az_ulib_ustream_read(az_ulib_ustream* ustream_instance, uint8_t* const buffer, size_t buffer_length, size_t* const size)
{
    return ustream_instance->control_block->api->read(ustream_instance, buffer, buffer_length, size);
}

/**
 * @brief   Returns the remaining size of the ustream.
 *
 *  This API returns the number of bytes between the current position and the end of the ustream.
 *
 *  The <tt>az_ulib_ustream_get_remaining_size</tt> API shall follow the following minimum requirements:
 *      - The <tt>get_remaining_size</tt> shall return the number of bytes between the current position and the
 *          end of the ustream.
 *      - If the provided interface is <tt>NULL</tt>, the <tt>get_remaining_size</tt> shall return #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR.
 *      - If the provided interface is not the implemented ustream type, the <tt>get_remaining_size</tt> shall
 *          return #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR.
 *      - If the provided size is <tt>NULL</tt>, the <tt>get_remaining_size</tt> shall return #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR.
 *
 * @param[in]   ustream_instance        The #az_ulib_ustream* with the interface of the ustream. It
 *                                      cannot be <tt>NULL</tt>, and it shall be a valid ustream that is the
 *                                      implemented ustream type.
 * @param[out]  size                    The <tt>size_t* const</tt> to return the remaining number of <tt>uint8_t</tt> values 
 *                                      It cannot be <tt>NULL</tt>.
 *
 * @return The #az_ulib_result with the result of the <tt>get_remaining_size</tt> operation.
 *          @retval     #AZ_ULIB_SUCCESS                If it succeeded to get the remaining size of the ustream.
 *          @retval     #AZ_ULIB_BUSY_ERROR             If the resource necessary to get the remaining size of
 *                                                        the ustream is busy.
 *          @retval     #AZ_ULIB_CANCELLED_ERROR        If the <tt>get_remaining_size</tt> was cancelled.
 *          @retval     #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR If one of the provided parameters is invalid.
 *          @retval     #AZ_ULIB_OUT_OF_MEMORY_ERROR    If there is not enough memory to execute the
 *                                                        <tt>get_remaining_size</tt> operation.
 *          @retval     #AZ_ULIB_SECURITY_ERROR         If the <tt>get_remaining_size</tt> was denied for security reasons.
 *          @retval     #AZ_ULIB_SYSTEM_ERROR           If the <tt>get_remaining_size</tt> operation failed on the
 *                                                        system level.
 */
static inline az_ulib_result az_ulib_ustream_get_remaining_size(az_ulib_ustream* ustream_instance, size_t* const size)
{
    return ustream_instance->control_block->api->get_remaining_size(ustream_instance, size);
}

/**
 * @brief   Returns the current position in the ustream.
 *
 *  This API returns the logical current position.
 *
 *  The <tt>az_ulib_ustream_get_position</tt> API shall follow the following minimum requirements:
 *      - The <tt>get_position</tt> shall return the logical current position of the ustream.
 *      - If the provided interface is <tt>NULL</tt>, the <tt>get_position</tt> shall return #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR.
 *      - If the provided interface is not the implemented ustream type, the <tt>get_position</tt>
 *          shall return #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR.
 *      - If the provided position is <tt>NULL</tt>, the <tt>get_position</tt> shall return #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR.
 *
 * @param[in]   ustream_instance    The #az_ulib_ustream* with the interface of the ustream. It
 *                                  cannot be <tt>NULL</tt>, and it shall be a valid ustream that is the
 *                                  implemented ustream type.
 * @param[out]  position            The <tt>offset_t* const</tt> to return the logical current position in the
 *                                  ustream. It cannot be <tt>NULL</tt>.
 *
 * @return The #az_ulib_result with the result of the <tt>get_position</tt> operation.
 *          @retval     #AZ_ULIB_SUCCESS                If it provided the position of the ustream.
 *          @retval     #AZ_ULIB_BUSY_ERROR             If the resource necessary for getting the
 *                                                        position is busy.
 *          @retval     #AZ_ULIB_CANCELLED_ERROR        If the <tt>get_position</tt> was cancelled.
 *          @retval     #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR If one of the provided parameters is invalid.
 *          @retval     #AZ_ULIB_OUT_OF_MEMORY_ERROR    If there is not enough memory to execute the
 *                                                        <tt>get_position</tt> operation.
 *          @retval     #AZ_ULIB_SECURITY_ERROR         If the <tt>get_position</tt> was denied for
 *                                                        security reasons.
 *          @retval     #AZ_ULIB_SYSTEM_ERROR           If the <tt>get_position</tt> operation failed on
 *                                                        the system level.
 */
static inline az_ulib_result az_ulib_ustream_get_position(az_ulib_ustream* ustream_instance, offset_t* const position)
{
    return ustream_instance->control_block->api->get_position(ustream_instance, position);
}

/**
 * @brief   Releases all the resources related to the <tt>Data Source</tt> before and including the released position.
 *
 *  Calling this API will notify the ustream that the developer will not need its content from the start
 *      to <tt>position</tt> (inclusive). It means that the implementation of the ustream can dispose
 *      any resources allocated to control and maintain this part of the ustream.
 *  It is up to the implementation of the ustream to decide to release any resource. For example, if the
 *      ustream is a string in the Flash, it does not make sense to release it.
 *  The provided position shall be the logical position, and it shall be between the logical first
 *      valid position of the ustream and the logical current position minus one. For example, the following
 *      code releases all bytes from the start to the last received position:
 *
 * @code
 * offset_t pos;
 * if(az_ulib_ustream_get_position(my_buffer, &pos) == AZ_ULIB_SUCCESS)
 * {
 *     az_ulib_ustream_release(my_buffer, pos - 1);
 * }
 * @endcode
 *
 *  The <tt>az_ulib_ustream_release</tt> API shall follow the following minimum requirements:
 *      - The <tt>release</tt> shall dispose all resources necessary to handle the content of ustream before and 
 *          including the release position.
 *      - If the release position is after the current position or the ustream size, the <tt>release</tt> shall
 *          return #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR, and do not release any resource.
 *      - If the release position is already released, the <tt>release</tt> shall return
 *          #AZ_ULIB_NO_SUCH_ELEMENT_ERROR, and do not release any resource.
 *      - If the provided interface is <tt>NULL</tt>, the <tt>release</tt> shall return #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR.
 *      - If the provided interface is not the implemented ustream type, the <tt>release</tt> shall return
 *          #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR.
 *
 * @param[in]  ustream_instance     The #az_ulib_ustream* with the interface of the ustream. It
 *                                  cannot be <tt>NULL</tt>, and it shall be a valid ustream that is the
 *                                  implemented ustream type.
 * @param[in]  position             The <tt>offset_t</tt> with the position in the ustream to release. The
 *                                  ustream will release the <tt>uint8_t</tt> on the position and all <tt>uint8_t</tt>
 *                                  before the position. It shall be bigger than 0.
 *
 * @return The #az_ulib_result with the result of the <tt>release</tt> operation.
 *          @retval     #AZ_ULIB_SUCCESS                If the ustream releases the position with success.
 *          @retval     #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR If one of the provided parameters is invalid.
 *          @retval     #AZ_ULIB_NO_SUCH_ELEMENT_ERROR  If the position is already released.
 *          @retval     #AZ_ULIB_SYSTEM_ERROR           If the <tt>release</tt> operation failed on the system level.
 */
static inline az_ulib_result az_ulib_ustream_release(az_ulib_ustream* ustream_instance, offset_t position)
{
    return ustream_instance->control_block->api->release(ustream_instance, position);
}

/**
 * @brief   Initializes a new instance of the ustream and returns it.
 *
 *  Cloning a ustream will create a new instance of the ustream that shares the same content of the
 *      original one. The clone shall not copy the content of the ustream, but only add a reference to
 *      it.
 *
 *  Both the start position and the current position of the cloned ustream will be the current
 *      position of the original ustream. The logical position of it will be determined by the provided
 *      offset.
 *
 *  The size of the new ustream will be the remaining size of the original ustream, which is the size
 *      minus the current position.
 *
 * <i><b>Example 1</b></i>
 *
 *  Consider a ustream with 1500 bytes, that was created from the factory, with <tt>Logical</tt> and <tt>Inner</tt>
 *      positions equal to <tt>0</tt>. After some operations, 1000 bytes were read (from 0 to 999). The
 *      current position is <tt>1000</tt>, and 200 bytes were released (from 0 to 199), so the released
 *      position is <tt>199</tt>.
 *  For the following examples, the positions are represented by <tt>[Logical, Inner]</tt>.
 *
 * Original ustream:
 *
 * @code
 *  |      Released     |             Pending             |               Future              |
 *  |-------------------|---------------------------------|-----------------------------------|
 *  |<- start [0, 0]    |<- released [199, 199]           |<- current [1000, 1000]            |<- end [1499, 1499]
 * @endcode
 *
 * Cloning the original ustream with offset 0 will result in the following ustream:
 *
 * @code
 *                       |||             Future                |
 *                       |||-----------------------------------|
 *  released [-1, 999] ->|||<- start, current [0, 1000]        |<- end [499, 1499]
 * @endcode
 *
 * Cloning the same original ustream with offset 100 will result in the following ustream:
 *
 * @code
 *                       |||             Future                |
 *                       |||-----------------------------------|
 *  released [99, 999] ->|||<- start, current [100, 1000]      |<- end [599, 1499]
 * @endcode
 *
 * <i><b>Example 2</b></i>
 *
 *  Consider a ustream with 5000 bytes, that was created from the factory, with <tt>Logical</tt> and <tt>Inner</tt>
 *      positions equal to <tt>0</tt>. After some operations, 250 bytes were read (from 0 to 249), so the
 *      current position is <tt>250</tt>, and no release was made, so the released position is still <tt>-1</tt>.
 *
 *  For the following examples, the positions are represented by <tt>[Logical, Inner]</tt>.
 *
 *
 * Original ustream:
 *
 * @code
 *                     ||     Pending       |                         Future                      |
 *                     ||-------------------+-----------------------------------------------------|
 *  released [-1, 0] ->||<- start [0, 0]    |<- current [250, 250]                                |<- end [4999, 4999]
 * @endcode
 *
 * Cloning this original ustream with offset 10000 will result in the following ustream:
 *
 * @code
 *                         |||                Future                |
 *                         |||--------------------------------------|
 *  released [9999, 249] ->|||<- start, current [10000, 250]        |<- end [14749, 4999]
 * @endcode
 *
 * <i><b>Example 3</b></i>
 *
 *  From the previous cloned ustream, after some operations, the <tt>Logical</tt> current position is moved
 *      to <tt>11000</tt>, and the <tt>Logical</tt> released position is <tt>10499</tt>.

 *  For the following examples, the positions are represented by <tt>[Logical, Inner]</tt>.
 *
 * Previous cloned ustream:
 *
 * @code
 *  |          Released       |           Pending          |               Future              |
 *  |-------------------------+----------------------------+-----------------------------------|
 *  |<- start [10000, 250]    |<- released [10499, 749]    |<- current [11000, 1250]           |<- end [14749, 4999]
 * @endcode
 *
 * Cloning this cloned ustream with offset 0 will result in the following ustream:
 *
 * @code
                          |||                Future                |
 *                        |||--------------------------------------|
 *  released [-1, 1249] ->|||<- start, current [0, 1250]           |<- end [3749, 4999]
 * @endcode
 *
 *  @note
 *  From the point of view of a consumer, the <tt>Inner</tt> position never matters, it will
 *      always use the <tt>Logical</tt> position for all operations.
 *  @note
 *  If the position is not important to the consumer, making the offset equal to <tt>0</tt> is a safe option.
 *
 *  The <tt>az_ulib_ustream_clone</tt> API shall follow the following minimum requirements:
 *      - The <tt>clone</tt> shall return a ustream with the same content of the original ustream.
 *      - If the provided interface is <tt>NULL</tt>, the <tt>clone</tt> shall return <tt>NULL</tt>.
 *      - If the provided interface is not a type of the implemented ustream, the <tt>clone</tt> shall return <tt>NULL</tt>.
 *      - If there is not enough memory to control the new ustream, the <tt>clone</tt> shall return <tt>NULL</tt>.
 *      - If the provided offset plus the ustream size is bigger than the maximum size_t, the <tt>clone</tt>
 *          shall return <tt>NULL</tt>.
 *      - The cloned ustream shall not interfere with the instance of the original ustream and vice versa.
 *
 * @param[out]  ustream_instance_clone      The #az_ulib_ustream* with the interface of the ustream.
 *                                          It cannot be <tt>NULL</tt>.
 * @param[in]   ustream_instance            The #az_ulib_ustream* to be cloned.
 *                                          It cannot be <tt>NULL</tt>, and it shall be a valid ustream instance type.
 * @param[out]  offset                      The <tt>offset_t</tt> with the <tt>Logical</tt> position of the first byte in
 *                                          the cloned ustream.
 *
 * @return The #az_ulib_ustream* with the result of the clone operation.
 *          @retval    not-NULL         If the ustream was cloned with success.
 *          @retval    NULL             If one of the provided parameters is invalid or there is not enough memory to
 *                                      control the new ustream.
 */
static inline az_ulib_result az_ulib_ustream_clone(az_ulib_ustream* ustream_instance_clone, az_ulib_ustream* ustream_instance, offset_t offset)
{
    return ustream_instance->control_block->api->clone(ustream_instance_clone, ustream_instance, offset);
}

/**
 * @brief   Release all the resources allocated to control the instance of the ustream.
 *
 *  The dispose will release the instance of the ustream and decrement the reference of the ustream.
 *      If there are no more references to the ustream, the dispose will release all resources
 *      allocated to control the ustream.
 *
 *  The <tt>az_ulib_ustream_dispose</tt> API shall follow the following minimum requirements:
 *      - The <tt>dispose</tt> shall free all allocated resources for the instance of the ustream.
 *      - If there are no more instances of the ustream, the <tt>dispose</tt> shall release all allocated
 *          resources to control the ustream.
 *      - If the provided interface is <tt>NULL</tt>, the <tt>dispose</tt> shall return #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR.
 *      - If the provided interface is not the type of the implemented ustream, the <tt>dispose</tt> shall return
 *          #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR.
 *
 * @param[in]   ustream_instance        The #az_ulib_ustream* with the interface of the ustream. It
 *                                      cannot be <tt>NULL</tt>, and it shall be a valid ustream that is a type
 *                                      of the implemented ustream.
 *
 * @return The #az_ulib_result with the result of the <tt>dispose</tt> operation.
 *          @retval #AZ_ULIB_SUCCESS                    If the instance of the ustream was disposed with success.
 *          @retval #AZ_ULIB_ILLEGAL_ARGUMENT_ERROR     If one of the provided parameters is invalid.
 */
static inline az_ulib_result az_ulib_ustream_dispose(az_ulib_ustream* ustream_instance)
{
    return ustream_instance->control_block->api->dispose(ustream_instance);
}


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* AZ_ULIB_USTREAM_BASE_H */
