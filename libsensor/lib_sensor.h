/*
 * Copyright (C) 2015, www.easyiot.com.cn
 *
 * The right to copy, distribute, modify, or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable license agreement.
 *
 */

#ifndef __LIB_SENSOR_H
#define __LIB_SENSOR_H

/**
 * 函数指针类型定义。
 *
 * sensor application 应该实现此类型的函数作为从物理 sensor 对应的数据点取得数据的处理函数，并将处理函数传递给 lib_sensor_start 调用。
 *
 * 参数说明：
 *
 *   prop_node:  指向数据点的属性指针，基于此指针，sensor application 可以用 libsensor 提供的辅助函数取得数据点的属性值
 *
 */
typedef void * dp_data_func_t(void *prop_node);

/**
 * 函数指针类型定义。
 *
 * sensor application 实现此类型的函数作为操作数据点所对应的开关，设备的处理函数，并将处理函数传递给 lib_sensor_start 调用。
 *
 * 参数说明：
 *
 *   prop_node:  指向数据点的属性指针，基于此指针，sensor application
 *   可以用 libsensor 提供的辅助函数取得数据点的属性值
 *
 *   data:  sensor application 自定义数据类型
 *
 */
typedef void set_dp_func_t(void *prop_node, void *data);

/**
 * libsensor 的入口函数
 *
 * sensor application 通过调用此入口函数，将进入 libsensor 的消息处理循环。根据数据点定义，libsensor 将会周期性调用 get_dp_func 从物理
 * 传感器、设备上获取数据。 同时根据来自 ithing 设备代理端下发的命令调用参数列表中列出的 set_dp_func 函数操作物理传感器、设备。
 *
 * 参数说明：
 *
 *   cfg_file:    sensor application 的配置文件路径及名称
 *
 *   get_dp_func: sensor application 自定义函数，将会被 libsensor 调用
 *   以获取设备数据
 *
 *   set_dp_func: sensor application 自定义函数，将会被 libsensor 调用
 *   以操作数据点
 *
 *   data:        sensor application 自定义数据类型，将会在 set_dp_func 函数被调用时作为其参数传入
 *
 * 行为说明：sensor application 调用此函数会进入 libsensor 的消息处理循环。只有在初始化错误，或者被用户中断执行（例如 control-c）时才回返回
 *   
 * 返回值：
 *
 *   -1：初始化错误
 *
 *    0：用户中断执行
 */
int lib_sensor_start(const char *cfg_file, dp_data_func_t *get_dp_func, set_dp_func_t *set_dp_func, void *data);

/**
 * libsensor 提供的辅助函数
 *
 * 根据数据点的属性名称获取属性值设置
 *
 * 参数说明：
 *
 *   node：    指向数据点的属性数据结构的指针
 *
 *   name：    数据点的属性名称
 *
 * 返回值： 指向属性名称所对应数据的指针
 */
void * get_node_by_name(void *node, const char *name);

/**
 * libsensor 提供的辅助函数
 *
 * 根据给定的数据点属性名称，取得对应的数据点属性所定义的 int 类型数值
 *
 * 参数说明：
 *
 *   node：    指向数据点的属性数据结构的指针
 *
 *   name：    数据点的属性名称
 *
 * 返回值： 指向属性名称所对应数值
 */
int get_int_by_name(void *node, const char *name);

/**
 * libsensor 提供的辅助函数
 *
 * 根据给定的数据点属性名称，取得对应的数据点属性所定义的 double 类型数值
 *
 * 参数说明：
 *
 *   node：    指向数据点的属性数据结构的指针
 *
 *   name：    数据点的属性名称
 *
 * 返回值： 指向属性名称所对应数值
 */
double get_double_by_name(void *node, const char *name);

/**
 * libsensor 提供的辅助函数
 *
 * 根据给定的数据点属性名称，取得对应的数据点属性所定义的 string 类型数值
 *
 * 参数说明：
 *
 *   node：    指向数据点的属性数据结构的指针
 *
 *   name：    数据点的属性名称
 *
 * 返回值： 指向属性名称所对应字符串的指针
 */
const char *get_string_by_name(void *node, const char *name);

/**
 * libsensor 提供的辅助函数
 *
 * 根据给定的属性名称，从 Sensor Application 全局属性定义中取得对应的属性所定义的 int 类型数值
 *
 * 参数说明：
 *
 *   name：    全局（global）属性名称
 *
 * 返回值： 指向属性名称所对应数值
 */
int int_from_config_by_name(const char *name);

/**
 * libsensor 提供的辅助函数
 *
 * 根据给定的数据点属性名称，取得对应的数据点属性所定义的 string 类型数值
 *
 * 参数说明：
 *
 *   name：     全局（global）属性名称
 *
 * 返回值： 指向属性名称所对应字符串的指针
 */
const char *string_from_config_by_name(const char *name);

#endif /* __LIB_SENSOR_H */
