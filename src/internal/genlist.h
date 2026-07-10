/**
 * @file   genlist.h
 * @brief  generic singly and doubly-linked list templates using macros
 */


#pragma once
#ifndef GENLIST_H
#define GENLIST_H


/**
 * @brief   defines sll and sll-coord structures
 * 
 * @param   _prefix prefix used for the sll and coord structs
 * @param   _type   the data type of the sll node
 *
 * @warning must be invoked before uses of `gen_sll_func`, `sll_coord` or `sll`
 */
#define gen_sll_struct(         \
        _prefix,                \
        _type                   \
    )                           \
                                \
    struct _prefix##_sll        \
    {                           \
        _type  *head;           \
    };                          \
                                \
    struct _prefix##_sll_coord  \
    {                           \
        _type  *next;           \
    };

#define sll(_prefix)            \
    struct _prefix##_sll

#define sll_coord(_prefix)      \
    struct _prefix##_sll_coord

/**
 * @brief   generates the push function for a singly linked list
 * 
 * @param   _attr         function attributes
 * @param   _prefix       prefix used for the generated function name
 * @param   _type         the data type of the list node
 * @param   _coord_field  name of the sll_coord struct member within _type
 */
#define _GEN_SLL_PUSH(                                                                              \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field                                                                                \
    )                                                                                               \
                                                                                                    \
    _attr void                                                                                      \
    _prefix##_push(                                                                                 \
        sll(_prefix)   *list,                                                                       \
        _type          *node                                                                        \
    )                                                                                               \
    {                                                                                               \
        if (!node)                                                                                  \
        {                                                                                           \
            return;                                                                                 \
        }                                                                                           \
                                                                                                    \
        node->_coord_field.next = list->head;                                                       \
        list->head              = node;                                                             \
    }

/**
 * @brief   generates the pop function for a singly linked list
 * 
 * @param   _attr         function attributes
 * @param   _prefix       prefix used for the generated function name
 * @param   _type         the data type of the list node
 * @param   _coord_field  name of the sll_coord struct member within _type
 */
#define _GEN_SLL_POP(                                                                               \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field                                                                                \
    )                                                                                               \
                                                                                                    \
    _attr _type*                                                                                    \
    _prefix##_pop(                                                                                  \
        sll(_prefix)   *list                                                                        \
    )                                                                                               \
    {                                                                                               \
        _type  *node;                                                                               \
                                                                                                    \
        node    = list->head;                                                                       \
        if (!node)                                                                                  \
        {                                                                                           \
            return nullptr;                                                                         \
        }                                                                                           \
                                                                                                    \
        list->head          = node->_coord_field.next;                                              \
        node->_coord_field  = (sll_coord(_prefix)){0};                                              \
                                                                                                    \
        return node;                                                                                \
    }


/**
 * @brief   generates the complete singly linked list structure and operational functions
 * 
 * @param   _attr         function attributes applied to all generated functions
 * @param   _prefix       prefix used for the list struct and function names
 * @param   _type         the data type of the list node
 * @param   _coord_field  name of the sll_coord struct member within _type
 */
#define gen_sll_func(                                                                               \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field                                                                                \
    )                                                                                               \
                                                                                                    \
    _GEN_SLL_PUSH(                                                                                  \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field                                                                                \
    )                                                                                               \
                                                                                                    \
    _GEN_SLL_POP(                                                                                   \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field                                                                                \
    )


/**
 * @brief   defines dll and dll-coord structures
 * 
 * @param   _prefix prefix used for the dll and coord structs
 * @param   _type   the data type of the dll node
 *
 * @warning must be invoked before uses of `gen_dll_func`, `dll_coord` or `dll`
 */
#define gen_dll_struct(         \
        _prefix,                \
        _type                   \
    )                           \
                                \
    struct _prefix##_dll        \
    {                           \
        _type  *head;           \
    };                          \
                                \
    struct _prefix##_dll_coord  \
    {                           \
        _type  *next;           \
    };

#define dll(_prefix)            \
    struct _prefix##_dll

#define dll_coord(_prefix)      \
    struct _prefix##_dll_coord

/**
 * @brief   generates the remove function for a doubly-linked list
 * 
 * @param   _attr         function attributes
 * @param   _prefix       prefix used for the generated function name
 * @param   _type         the data type of the list node
 * @param   _coord_field  name of the dll_coord struct member within _type
 */
#define _GEN_DLL_REMOVE(                                                                            \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field                                                                                \
    )                                                                                               \
                                                                                                    \
    _attr void                                                                                      \
    _prefix##_remove(                                                                               \
        dll(_prefix)   *list,                                                                       \
        _type          *node                                                                        \
    )                                                                                               \
    {                                                                                               \
        if (!node)                                                                                  \
        {                                                                                           \
            return;                                                                                 \
        }                                                                                           \
                                                                                                    \
        if (node->_coord_field.prev)                                                                \
        {                                                                                           \
            node->_coord_field.prev->_coord_field.next  = node->_coord_field.next;                  \
        }                                                                                           \
        else                                                                                        \
        {                                                                                           \
            list->head  = node->_coord_field.next;                                                  \
        }                                                                                           \
                                                                                                    \
        if (node->_coord_field.next)                                                                \
        {                                                                                           \
            node->_coord_field.next->_coord_field.prev  = node->_coord_field.prev;                  \
        }                                                                                           \
        else                                                                                        \
        {                                                                                           \
            list->tail  = node->_coord_field.prev;                                                  \
        }                                                                                           \
                                                                                                    \
        node->_coord_field  = (dll_coord(_prefix)){0};                                              \
    }


/**
 * @brief   generates the push_front function for a doubly-linked list
 * 
 * @param   _attr         function attributes
 * @param   _prefix       prefix used for the generated function name
 * @param   _type         the data type of the list node
 * @param   _coord_field  name of the dll_coord struct member within _type
 */
#define _GEN_DLL_PUSH_FRONT(                                                                        \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field                                                                                \
    )                                                                                               \
                                                                                                    \
    _attr void                                                                                      \
    _prefix##_push_front(                                                                           \
        dll(_prefix)  *list,                                                                        \
        _type          *node                                                                        \
    )                                                                                               \
    {                                                                                               \
        if (!node)                                                                                  \
        {                                                                                           \
            return;                                                                                 \
        }                                                                                           \
                                                                                                    \
        node->_coord_field.next = list->head;                                                       \
        node->_coord_field.prev = nullptr;                                                          \
                                                                                                    \
        if (list->head)                                                                             \
        {                                                                                           \
            list->head->_coord_field.prev   = node;                                                 \
        }                                                                                           \
        else                                                                                        \
        {                                                                                           \
            list->tail  = node;                                                                     \
        }                                                                                           \
                                                                                                    \
        list->head  = node;                                                                         \
    }


/**
 * @brief   generates the push_back function for a doubly-linked list
 * 
 * @param   _attr         function attributes
 * @param   _prefix       prefix used for the generated function name
 * @param   _type         the data type of the list node
 * @param   _coord_field  name of the dll_coord struct member within _type
 */
#define _GEN_DLL_PUSH_BACK(                                                                         \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field                                                                                \
    )                                                                                               \
                                                                                                    \
    _attr void                                                                                      \
    _prefix##_push_back(                                                                            \
        dll(_prefix)   *list,                                                                       \
        _type          *node                                                                        \
    )                                                                                               \
    {                                                                                               \
        if (!node)                                                                                  \
        {                                                                                           \
            return;                                                                                 \
        }                                                                                           \
                                                                                                    \
        node->_coord_field.next = nullptr;                                                          \
        node->_coord_field.prev = list->tail;                                                       \
                                                                                                    \
        if (list->tail)                                                                             \
        {                                                                                           \
            list->tail->_coord_field.next   = node;                                                 \
        }                                                                                           \
        else                                                                                        \
        {                                                                                           \
            list->head  = node;                                                                     \
        }                                                                                           \
                                                                                                    \
        list->tail  = node;                                                                         \
    }


/**
 * @brief   generates the pop_front function for a doubly-linked list
 * 
 * @param   _attr         function attributes
 * @param   _prefix       prefix used for the generated function name
 * @param   _type         the data type of the list node
 * @param   _coord_field  name of the dll_coord struct member within _type
 */
#define _GEN_DLL_POP_FRONT(                                                                         \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field                                                                                \
    )                                                                                               \
                                                                                                    \
    _attr _type*                                                                                    \
    _prefix##_pop_front(                                                                            \
        dll(_prefix)   *list                                                                        \
    ){                                                                                              \
        _type  *node;                                                                               \
                                                                                                    \
        node    = list->head;                                                                       \
        if (node)                                                                                   \
        {                                                                                           \
            _prefix##_remove(list, node);                                                           \
        }                                                                                           \
                                                                                                    \
        return node;                                                                                \
    }


/**
 * @brief   generates the pop_back function for a doubly-linked list
 * 
 * @param   _attr         function attributes
 * @param   _prefix       prefix used for the generated function name
 * @param   _type         the data type of the list node
 * @param   _coord_field  name of the dll_coord struct member within _type
 */
#define _GEN_DLL_POP_BACK(                                                                          \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field                                                                                \
    )                                                                                               \
                                                                                                    \
    _attr _type*                                                                                    \
    _prefix##_pop_back(                                                                             \
        dll(_prefix)   *list                                                                        \
    )                                                                                               \
    {                                                                                               \
        _type  *node;                                                                               \
                                                                                                    \
        node    = list->tail;                                                                       \
        if (node)                                                                                   \
        {                                                                                           \
            _prefix##_remove(list, node);                                                           \
        }                                                                                           \
                                                                                                    \
        return node;                                                                                \
    }


/**
 * @brief   generates the complete doubly-linked list structure and operational functions
 * 
 * @param   _attr         function attributes applied to all generated functions
 * @param   _prefix       prefix used for the list struct and function names
 * @param   _type         the data type of the list node
 * @param   _coord_field  name of the dll_coord struct member within _type
 */
#define gen_dll_func(                                                                               \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field                                                                                \
    )                                                                                               \
                                                                                                    \
    _GEN_DLL_REMOVE(                                                                                \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field                                                                                \
    )                                                                                               \
                                                                                                    \
    _GEN_DLL_PUSH_FRONT(                                                                            \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field                                                                                \
    )                                                                                               \
                                                                                                    \
    _GEN_DLL_PUSH_BACK(                                                                             \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field                                                                                \
    )                                                                                               \
                                                                                                    \
    _GEN_DLL_POP_FRONT(                                                                             \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field                                                                                \
    )                                                                                               \
                                                                                                    \
    _GEN_DLL_POP_BACK(                                                                              \
        _attr,                                                                                      \
        _prefix,                                                                                    \
        _type,                                                                                      \
        _coord_field                                                                                \
    )


#endif  //GENLIST_H