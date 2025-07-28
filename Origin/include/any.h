#ifndef __ANY_H
#define __ANY_H

#include <memory>
#include <stdexcept>

/*
    该Any类通过**模板和继承机制**实现了类型擦除，允许存储任意类型的数据，并在需要时安全地取回。
    它使用了**多态和运行时类型识别（RTTI）**来实现类型安全的转换。虽然简单，但展示了类型擦除的基本原理
*/
/*
    原理：类型擦除
        - 将任意类型的数据包装在一个派生类模板Derive<T>中，该派生类继承自公共基类Base
        - 使用基类指针（std::unique_ptr<Base>）来管理这个派生类对象，从而擦除了原始数据的类型信息
        - 当需要取回原始数据时，通过dynamic_cast尝试将基类指针转换回具体的派生类指针。
          如果类型匹配（即T与当初存储的类型一致），则转换成功，可以取出数据；否则，转换失败，抛出异常

    安全性：
        - 使用dynamic_cast进行运行时类型检查，确保类型安全。如果类型不匹配，会返回nullptr（对于指针转换），从而可以检测并处理错误
        - 由于使用了虚析构函数，通过基类指针删除派生类对象是安全的
*/
class Any
{
public:
    // 默认构造
    Any() = default;

    // 构造函数
    template<typename T>
    Any(T data)
        : base_(std::make_unique<Derive<T>>(data))
    {}

    // 默认析构
    ~Any() = default;

    Any(const Any&) = delete;
    Any& operator=(const Any&) = delete;

    Any(Any&&) = default;
    Any& operator=(Any&&) = default;

    // 空状态检查
    bool has_value() const {
        return base_ != nullptr;
    }

    // 提取Any对象存储的具体数据类型
    template<typename T>
    T cast() {
        // 通过base_获取指向派生类对象中的data_成员变量
        // 基类指针转为派生类指针，dynamic_cast
        /*
            C++中，RTTI（Run-Time Type Information）是用于在程序运行时识别对象类型信息的机制
            核心组件：
                - typeid运算符
                    - 返回一个std::type_info对象；
                    - 可获取类型的名称（type_info::name()）；
                    - 支持类型比较（type_info::operator==）
                - dynamic_cast运算符
                    - 用于安全地沿继承层次进行向下转型
                    - 失败时返回nullptr（指针）或抛出std::bad_cast（引用）
                - 性能影响
                    - dynamic_cast在复杂继承层次中可能较慢（需遍历继承树）
        */
        // base_.get()：返回基类裸指针
        Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
        if(pd == nullptr) {
            throw std::bad_cast();
        }

        return pd->data_;
    }

    // 
    template<typename T>
    T cast() const {
        Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
        if(pd == nullptr) {
            throw std::bad_cast();
        }

        return pd->data_;
    }

private:
    // 内部抽象基类类型
    // 有一个虚构函数（用于多态）
    class Base
    {
    public:
        virtual ~Base() = default;
    };

    // 派生类类型
    template<typename T>
    class Derive : public Base
    {
    public:
        Derive(T data)
            : data_(data)
        {}

        T data_;                    // 保存任意数据类型
    };

private:
    std::unique_ptr<Base> base_;    // 基类指针
};

#endif