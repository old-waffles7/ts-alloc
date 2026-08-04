/**
 * @file   genheap.h
 * @brief  generic pairing heap templete using macros
 */


#pragma once
#ifndef GENHEAP_H
#define GENHEAP_H


/**
 * @brief   generates the merge function to combine two pairing heaps
 * 
 * @param   _attr         function attributes (e.g., static inline)
 * @param   _prefix       prefix used for the generated function name
 * @param   _type         the data type of the heap node
 * @param   _coord_field  name of the heap_coord struct member within _type
 * @param   _cmp_func     comparison function to determine node priority
 */
#define _GEN_HEAP_MERGE(                                                                            \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field,                                                                               \
        _cmp_func                                                                                   \
    )                                                                                               \
                                                                                                    \
    _attr _type*                                                                                    \
    _prefix##_merge(                                                                                \
        _type  *node_1,                                                                             \
        _type  *node_2                                                                              \
    )                                                                                               \
    {                                                                                               \
        if (!node_1)                                                                                \
        {                                                                                           \
            return node_2;                                                                          \
        }                                                                                           \
        if (!node_2)                                                                                \
        {                                                                                           \
            return node_1;                                                                          \
        }                                                                                           \
                                                                                                    \
        _type  *_root;                                                                              \
        _type  *_child;                                                                             \
                                                                                                    \
        if (_cmp_func(node_1, node_2))                                                              \
        {                                                                                           \
            _root   = node_1;                                                                       \
            _child  = node_2;                                                                       \
        }                                                                                           \
        else                                                                                        \
        {                                                                                           \
            _root   = node_2;                                                                       \
            _child  = node_1;                                                                       \
        }                                                                                           \
                                                                                                    \
        if (_root->_coord_field.childlist.head)                                                     \
        {                                                                                           \
            _root->_coord_field.childlist.head->_coord_field.prev   = _child;                       \
        }                                                                                           \
                                                                                                    \
        _child->_coord_field.prev           = _root;                                                \
        _child->_coord_field.next           = _root->_coord_field.childlist.head;                   \
        _root->_coord_field.childlist.head  = _child;                                               \
        _root->_coord_field.prev            = nullptr;                                              \
        _root->_coord_field.next            = nullptr;                                              \
                                                                                                    \
        return _root;                                                                               \
    }

/**
 * @brief   generates the pop function to extract the root node
 * 
 * @param   _attr         function attributes
 * @param   _prefix       prefix used for the generated function name
 * @param   _type         the data type of the heap node
 * @param   _coord_field  name of the heap_coord struct member within _type
 */
#define _GEN_HEAP_POP(                                                                              \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field                                                                                \
    )                                                                                               \
                                                                                                    \
    _attr _type*                                                                                    \
    _prefix##_pop(                                                                                  \
        heap(_prefix)  *heap                                                                        \
    )                                                                                               \
    {                                                                                               \
        _type  *root;                                                                               \
        _type  *tail;                                                                               \
        _type  *curr;                                                                               \
                                                                                                    \
        root    = heap->root;                                                                       \
        if (!root)                                                                                  \
        {                                                                                           \
            return nullptr;                                                                         \
        }                                                                                           \
                                                                                                    \
        curr    = root->_coord_field.childlist.head;                                                \
        if (!curr)                                                                                  \
        {                                                                                           \
            heap->root          = nullptr;                                                          \
            root->_coord_field  = (heap_coord(_prefix)){0};                                         \
            return root;                                                                            \
        }                                                                                           \
        tail    = nullptr;                                                                          \
                                                                                                    \
        while (curr && curr->_coord_field.next)                                                     \
        {                                                                                           \
            _type  *merge;                                                                          \
            _type  *pair_l;                                                                         \
            _type  *pair_r;                                                                         \
            _type  *next_pair;                                                                      \
                                                                                                    \
            pair_l      = curr;                                                                     \
            pair_r      = pair_l->_coord_field.next;                                                \
            next_pair   = pair_r->_coord_field.next;                                                \
            merge       = _prefix##_merge(pair_l, pair_r);                                          \
                                                                                                    \
            merge->_coord_field.prev    = tail;                                                     \
            if (tail)                                                                               \
            {                                                                                       \
                tail->_coord_field.next = merge;                                                    \
            }                                                                                       \
                                                                                                    \
            tail    = merge;                                                                        \
            curr    = next_pair;                                                                    \
        }                                                                                           \
                                                                                                    \
        if (curr)                                                                                   \
        {                                                                                           \
            if (tail)                                                                               \
            {                                                                                       \
                tail->_coord_field.next = curr;                                                     \
            }                                                                                       \
            curr->_coord_field.prev = tail;                                                         \
            tail                    = curr;                                                         \
        }                                                                                           \
                                                                                                    \
        while (tail && tail->_coord_field.prev)                                                     \
        {                                                                                           \
            _type  *prev;                                                                           \
            _type  *prev_prev;                                                                      \
                                                                                                    \
            prev                    = tail->_coord_field.prev;                                      \
            prev_prev               = prev->_coord_field.prev;                                      \
                                                                                                    \
            tail->_coord_field.prev = nullptr;                                                      \
            prev->_coord_field.next = nullptr;                                                      \
                                                                                                    \
            tail                    = _prefix##_merge(prev, tail);                                  \
                                                                                                    \
            tail->_coord_field.prev = prev_prev;                                                    \
            if (prev_prev) {                                                                        \
                prev_prev->_coord_field.next = tail;                                                \
            }                                                                                       \
        }                                                                                           \
                                                                                                    \
        heap->root          = tail;                                                                 \
        root->_coord_field  = (heap_coord(_prefix)){0};                                             \
                                                                                                    \
        return root;                                                                                \
    }

/**
 * @brief   generates the insert function to add a node to the heap
 * 
 * @param   _attr         function attributes
 * @param   _prefix       prefix used for the generated function name
 * @param   _type         the data type of the heap node
 * @param   _coord_field  name of the heap_coord struct member within _type
 */
#define _GEN_HEAP_INSERT(                                                                           \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field                                                                                \
    )                                                                                               \
                                                                                                    \
    _attr void                                                                                      \
    _prefix##_insert(                                                                               \
        heap(_prefix)  *heap,                                                                       \
        _type          *node                                                                        \
    )                                                                                               \
    {                                                                                               \
        if (!node)                                                                                  \
        {                                                                                           \
            return;                                                                                 \
        }                                                                                           \
                                                                                                    \
        node->_coord_field  = (heap_coord(_prefix)){0};                                             \
        heap->root          = _prefix##_merge(heap->root, node);                                    \
    }

/**
 * @brief   generates the remove function to extract an arbitrary node from the heap
 * 
 * @param   _attr         function attributes
 * @param   _prefix       prefix used for the generated function name
 * @param   _type         the data type of the heap node
 * @param   _coord_field  name of the heap_coord struct member within _type
 */
#define _GEN_HEAP_REMOVE(                                                                           \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field                                                                                \
    )                                                                                               \
                                                                                                    \
    _attr void                                                                                      \
    _prefix##_remove(                                                                               \
        heap(_prefix)  *heap,                                                                       \
        _type          *node                                                                        \
    )                                                                                               \
    {                                                                                               \
        if (!node)                                                                                  \
        {                                                                                           \
            return;                                                                                 \
        }                                                                                           \
                                                                                                    \
        if (heap->root == node)                                                                     \
        {                                                                                           \
            _prefix##_pop(heap);                                                                    \
            return;                                                                                 \
        }                                                                                           \
                                                                                                    \
        if (node->_coord_field.prev->_coord_field.childlist.head == node)                           \
        {                                                                                           \
            node->_coord_field.prev->_coord_field.childlist.head    = node->_coord_field.next;      \
        }                                                                                           \
        else                                                                                        \
        {                                                                                           \
            node->_coord_field.prev->_coord_field.next  = node->_coord_field.next;                  \
        }                                                                                           \
                                                                                                    \
        if (node->_coord_field.next)                                                                \
        {                                                                                           \
            node->_coord_field.next->_coord_field.prev  = node->_coord_field.prev;                  \
        }                                                                                           \
                                                                                                    \
        struct _prefix##_heap   temp_heap;                                                          \
                                                                                                    \
        temp_heap.root  = node;                                                                     \
        _prefix##_pop(&temp_heap);                                                                  \
                                                                                                    \
        heap->root  = _prefix##_merge(heap->root, temp_heap.root);                                  \
                                                                                                    \
        node->_coord_field  = (heap_coord(_prefix)){0};                                             \
    }


/**
 * @brief   defines pairing-heap and pairing-heap coord structures
 * 
 * @param   _prefix prefix used for the heap and coord structs
 * @param   _type   the data type of the heap node
 *
 * @warning must be invoked before uses of `gen_heap_func`, `heap_coord` or `heap
 */
#define gen_heap_struct(        \
        _prefix,                \
        _type                   \
    )                           \
                                \
    struct _prefix##_heap       \
    {                           \
        _type  *root;           \
    };                          \
                                \
    struct _prefix##_heap_coord \
    {                           \
        _type  *next;           \
        _type  *prev;           \
        struct                  \
        {                       \
            _type  *head;       \
        } childlist;            \
    };      

/**
 * @brief   generates pairing-heap functionalities
 * 
 * @param   _attr         function attributes applied to all generated functions
 * @param   _prefix       prefix used for the heap struct and function names
 * @param   _type         the data type of the heap node
 * @param   _coord_field  name of the heap_coord struct member within _type
 * @param   _cmp_func     comparison function to determine node priority
 */
#define gen_heap_func(                                                                              \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field,                                                                               \
        _cmp_func                                                                                   \
    )                                                                                               \
                                                                                                    \
    _GEN_HEAP_MERGE(                                                                                \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field,                                                                               \
        _cmp_func                                                                                   \
    )                                                                                               \
                                                                                                    \
    _GEN_HEAP_POP(                                                                                  \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field                                                                                \
    )                                                                                               \
                                                                                                    \
    _GEN_HEAP_INSERT(                                                                               \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field                                                                                \
    )                                                                                               \
                                                                                                    \
    _GEN_HEAP_REMOVE(                                                                               \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field                                                                                \
    )

#define heap(_prefix)           \
    struct _prefix##_heap 


#define heap_coord(_prefix)     \
    struct _prefix##_heap_coord  
    

#endif  //GENHEAP_H