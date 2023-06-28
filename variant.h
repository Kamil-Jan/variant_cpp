#pragma once

#include <algorithm>
#include <any>
#include <array>
#include <functional>
#include <iostream>
#include <new>
#include <utility>

namespace variant_util {
constexpr size_t NPOS = -1;

template <size_t Index, typename T, typename...>
struct get_index_by_type {
    static const size_t value = NPOS;
};

template <size_t Index, typename T, typename Head, typename... Tail>
struct get_index_by_type<Index, T, Head, Tail...> {
    static const size_t value =
        std::is_same_v<T, Head>
            ? Index
            : get_index_by_type<Index + 1, T, Tail...>::value;
};

template <typename... Types>
constexpr size_t get_index_by_type_v = get_index_by_type<0, Types...>::value;

struct Empty {};

template <size_t Index, typename...>
struct get_type_by_index {
    using type = Empty;
};

template <size_t Index, typename Head, typename... Tail>
struct get_type_by_index<Index, Head, Tail...> {
    using type = std::conditional_t<
        Index == 0, Head, typename get_type_by_index<Index - 1, Tail...>::type>;
};

template <size_t Index, typename... Types>
using get_type_by_index_t = typename get_type_by_index<Index, Types...>::type;
}  // namespace variant_util

using variant_util::get_index_by_type_v;
using variant_util::get_type_by_index_t;
using variant_util::NPOS;

template <typename... Types>
class Variant;

template <size_t Index, typename... Types>
const auto& Get(const Variant<Types...>& v);

template <size_t Index, typename... Types>
auto& Get(Variant<Types...>& v);

template <size_t Index, typename... Types>
auto&& Get(Variant<Types...>&& v);

template <size_t Index, typename... Types>
const auto&& Get(const Variant<Types...>&& v);

template <typename...>
union VariadicUnion {
    template <size_t Index>
    void get() const {
        static_assert(Index == 0, "Invalid index or type!");
    }

    template <size_t Index, typename T, typename... Args>
    void put(Args&&... /*unused*/) {
        static_assert(Index == 0, "Invalid index or type!");
    }

    template <size_t Index, typename T>
    void assign(T&& /*unused*/) {
        static_assert(Index == 0, "Invalid index or type!");
    }
};

template <typename Head, typename... Tail>
union VariadicUnion<Head, Tail...> {
    Head head;
    VariadicUnion<Tail...> tail;

    VariadicUnion() {}

    ~VariadicUnion() {}

    template <size_t Index>
    const auto& get() const {
        if constexpr (Index == 0) {
            return head;
        } else {
            return tail.template get<Index - 1>();
        }
    }

    template <size_t Index>
    auto& get() {
        if constexpr (Index == 0) {
            return head;
        } else {
            return tail.template get<Index - 1>();
        }
    }

    template <size_t Index, typename T, typename... Args>
    void put(Args&&... args) {
        if constexpr (std::is_same_v<T, Head>) {
            new (const_cast<std::remove_const_t<Head>*>(std::launder(&head)))
                Head(std::forward<Args>(args)...);
        } else {
            tail.template put<Index + 1, T>(std::forward<Args>(args)...);
        }
    }

    template <size_t Index, typename T>
    void assign(const T& value) {
        if constexpr (std::is_same_v<std::remove_reference_t<T>, Head>) {
            head = value;
        } else {
            tail.template assign<Index + 1, T>(value);
        }
    }

    template <size_t Index, typename T>
    void assign(T&& value) {
        if constexpr (std::is_same_v<std::remove_reference_t<T>, Head>) {
            head = std::forward<T>(value);
        } else {
            tail.template assign<Index + 1, T>(std::forward<T>(value));
        }
    }

    template <typename T>
    void destroy() {
        if constexpr (std::is_same_v<T, Head>) {
            head.~Head();
        } else {
            tail.template destroy<T>();
        }
    }
};

template <typename T, typename... Types>
struct VariantAlternative {
    using Derived = Variant<Types...>;
    static const size_t Index = get_index_by_type_v<T, Types...>;

    VariantAlternative() {}

    ~VariantAlternative() {}

    template <typename U = T>
        requires std::is_same_v<U, std::string>
    VariantAlternative(const char* value) {
        auto this_ptr = static_cast<Derived*>(this);
        this_ptr->storage.template put<0, T>(value);
        this_ptr->idx = Index;
    }

    VariantAlternative(const T& value) {
        auto this_ptr = static_cast<Derived*>(this);
        this_ptr->storage.template put<0, T>(value);
        this_ptr->idx = Index;
    }

    VariantAlternative(T&& value) {
        auto this_ptr = static_cast<Derived*>(this);
        this_ptr->storage.template put<0, T>(std::move(value));
        this_ptr->idx = Index;
    }

    VariantAlternative(const VariantAlternative& other) {
        auto other_ptr = static_cast<const Derived*>(&other);
        if (other_ptr->idx == Index) {
            auto this_ptr = static_cast<Derived*>(this);
            this_ptr->storage.template put<0, T>(
                other_ptr->storage.template get<Index>());
            this_ptr->idx = Index;
        }
    }

    VariantAlternative(VariantAlternative&& other) {
        auto other_ptr = static_cast<Derived*>(&other);
        if (other_ptr->idx == Index) {
            auto this_ptr = static_cast<Derived*>(this);
            this_ptr->storage.template put<0, T>(
                std::move(other_ptr->storage.template get<Index>()));
            this_ptr->idx = Index;
        }
    }

    template <typename U = T>
        requires std::is_same_v<U, std::string>
    Derived& operator=(const char* value) {
        auto this_ptr = static_cast<Derived*>(this);
        if (Index == this_ptr->idx) {
            this_ptr->storage.template assign<0, T>(value);
        } else {
            this_ptr->destroy();
            this_ptr->storage.template put<0, T>(value);
            this_ptr->idx = Index;
        }
        return *this_ptr;
    }

    Derived& operator=(const T& value) {
        auto this_ptr = static_cast<Derived*>(this);
        if (Index == this_ptr->idx) {
            this_ptr->storage.template assign<0, T>(value);
        } else {
            this_ptr->destroy();
            this_ptr->storage.template put<0, T>(value);
            this_ptr->idx = Index;
        }
        return *this_ptr;
    }

    Derived& operator=(T&& value) {
        auto this_ptr = static_cast<Derived*>(this);
        if (Index == this_ptr->idx) {
            this_ptr->storage.template assign<0, T>(std::move(value));
        } else {
            this_ptr->destroy();
            this_ptr->storage.template put<0, T>(std::move(value));
            this_ptr->idx = Index;
        }
        return *this_ptr;
    }

    template <typename... Args>
    T& emplace(Args&&... args) {
        auto this_ptr = static_cast<Derived*>(this);
        try {
            destroy();
            this_ptr->storage.template put<0, T>(std::forward<Args>(args)...);
            this_ptr->idx = Index;
        } catch (...) {
            this_ptr->idx = NPOS;
            throw;
        }
        return this_ptr->storage.template get<Index>();
    }

    void assign(const VariantAlternative& other) {
        auto other_ptr = static_cast<const Derived*>(&other);
        if (Index != other_ptr->idx) {
            return;
        }
        auto this_ptr = static_cast<Derived*>(this);
        this_ptr->storage.template put<0, T>(Get<Index>(*other_ptr));
        this_ptr->idx = Index;
    }

    void assign(VariantAlternative&& other) {
        auto other_ptr = static_cast<Derived*>(&other);
        if (Index != other_ptr->idx) {
            return;
        }
        auto this_ptr = static_cast<Derived*>(this);
        this_ptr->storage.template put<0, T>(std::move(Get<Index>(*other_ptr)));
        this_ptr->idx = Index;
    }

    void destroy() {
        auto this_ptr = static_cast<Derived*>(this);
        if (this_ptr->idx == Index) {
            this_ptr->storage.template destroy<T>();
        }
    }
};

template <typename... Types>
class Variant : VariantAlternative<Types, Types...>... {
  private:
    template <typename T, typename... Ts>
    friend struct VariantAlternative;

    template <size_t Index, typename... Ts>
    friend const auto& Get(const Variant<Ts...>& v);

    template <size_t Index, typename... Ts>
    friend auto& Get(Variant<Ts...>& v);

    template <size_t Index, typename... Ts>
    friend auto&& Get(Variant<Ts...>&& v);

    template <size_t Index, typename... Ts>
    friend const auto&& Get(const Variant<Ts...>&& v);

    template <typename T, typename... Ts>
    friend const T& Get(const Variant<Ts...>& v);

    template <typename T, typename... Ts>
    friend T& Get(Variant<Ts...>& v);

    template <typename T, typename... Ts>
    friend T&& Get(Variant<Ts...>&& v);

    template <typename T, typename... Ts>
    friend const T&& Get(const Variant<Ts...>&& v);

    template <typename T, typename... Ts>
    friend bool holds_alternative(Variant<Ts...>& v);

    VariadicUnion<Types...> storage;
    size_t idx;

  public:
    using VariantAlternative<Types, Types...>::VariantAlternative...;
    using VariantAlternative<Types, Types...>::operator=...;

    Variant() : idx(0) {
        storage.template put<0, get_type_by_index_t<0, Types...>>();
    }

    Variant(const Variant& other)
        : VariantAlternative<Types, Types...>(other)... {};

    Variant(Variant&& other)
        : VariantAlternative<Types, Types...>(std::move(other))... {};

    ~Variant() {
        destroy();
    }

    Variant& operator=(const Variant& other) {
        destroy();
        (VariantAlternative<Types, Types...>::assign(other), ...);
        return *this;
    }

    Variant& operator=(Variant&& other) {
        destroy();
        (VariantAlternative<Types, Types...>::assign(std::move(other)), ...);
        return *this;
    }

    template <typename T, typename... Args>
    T& emplace(Args&&... args) {
        constexpr size_t new_idx =
            get_index_by_type_v<std::remove_reference_t<T>, Types...>;
        try {
            destroy();
            storage.template put<0, T>(std::forward<Args>(args)...);
            idx = new_idx;
        } catch (...) {
            idx = NPOS;
            throw;
        }
        return storage.template get<new_idx>();
    }

    template <size_t Index, typename... Args>
    auto& emplace(Args&&... args) {
        return emplace<get_type_by_index_t<Index, Types...>>(
            std::forward<Args>(args)...);
    }

    template <typename T, typename U, typename... Args>
    T& emplace(std::initializer_list<U> list, Args&&... args) {
        constexpr size_t new_idx =
            get_index_by_type_v<std::remove_reference_t<T>, Types...>;
        try {
            destroy();
            storage.template put<0, T>(list, std::forward<Args>(args)...);
            idx = new_idx;
        } catch (...) {
            idx = NPOS;
            throw;
        }
        return storage.template get<new_idx>();
    }

    template <size_t Index, typename U, typename... Args>
    auto& emplace(std::initializer_list<U> list, Args&&... args) {
        return emplace<get_type_by_index_t<Index, Types...>>(
            list, std::forward<Args>(args)...);
    }

    constexpr size_t index() const {
        return idx;
    }

    bool valueless_by_exception() const {
        return idx == NPOS;
    }

  private:
    void destroy() {
        (VariantAlternative<Types, Types...>::destroy(), ...);
    }
};

template <size_t Index, typename... Types>
const auto& Get(const Variant<Types...>& v) {
    if (v.idx != Index) {
        throw std::runtime_error("Bad variant access!");
    }
    return v.storage.template get<Index>();
}

template <size_t Index, typename... Types>
auto& Get(Variant<Types...>& v) {
    if (v.idx != Index) {
        throw std::runtime_error("Bad variant access!");
    }
    return v.storage.template get<Index>();
}

template <size_t Index, typename... Types>
auto&& Get(Variant<Types...>&& v) {
    if (v.idx != Index) {
        throw std::runtime_error("Bad variant access!");
    }
    return std::move(v.storage.template get<Index>());
}

template <size_t Index, typename... Types>
const auto&& Get(const Variant<Types...>&& v) {
    if (v.idx != Index) {
        throw std::runtime_error("Bad variant access!");
    }
    return std::move(v.storage.template get<Index>());
}

template <typename T, typename... Types>
const T& Get(const Variant<Types...>& v) {
    return Get<get_index_by_type_v<T, Types...>>(v);
}

template <typename T, typename... Types>
T& Get(Variant<Types...>& v) {
    return Get<get_index_by_type_v<T, Types...>>(v);
}

template <typename T, typename... Types>
T&& Get(Variant<Types...>&& v) {
    return std::move(Get<get_index_by_type_v<T, Types...>>(std::move(v)));
}

template <typename T, typename... Types>
const T&& Get(const Variant<Types...>&& v) {
    return std::move(Get<get_index_by_type_v<T, Types...>>(std::move(v)));
}

template <typename T, typename... Types>
bool holds_alternative(Variant<Types...>& v) {
    return get_index_by_type_v<T, Types...> == v.idx;
}

template <typename T>
struct variant_size {
    static const size_t value = -1;
};

template <typename... Types>
struct variant_size<Variant<Types...>> {
    static const size_t value = sizeof...(Types);
};

// https://stackoverflow.com/a/50944602
template <typename Dest = void, typename... Arg>
constexpr auto make_array(Arg&&... arg) {
    if constexpr (std::is_same<void, Dest>::value) {
        return std::array<std::common_type_t<std::decay_t<Arg>...>,
                          sizeof...(Arg)>{{std::forward<Arg>(arg)...}};
    } else {
        return std::array<Dest, sizeof...(Arg)>{{std::forward<Arg>(arg)...}};
    }
}

// https://mpark.github.io/programming/2015/07/07/variant-visitation/
template <typename F, typename... Vs, size_t... Is>
constexpr auto make_fmatrix_impl(std::index_sequence<Is...> /*unused*/) {
    struct dispatcher {
        static constexpr decltype(auto) dispatch(F&& f, Vs&&... vs) {
            return std::invoke(static_cast<F>(f),
                               Get<Is>(static_cast<Vs>(vs))...);
        }
    };
    return &dispatcher::dispatch;
}

template <typename F, typename... Vs, size_t... Is, size_t... Js,
          typename... Ls>
constexpr auto make_fmatrix_impl(std::index_sequence<Is...> /*unused*/,
                                 std::index_sequence<Js...> /*unused*/,
                                 Ls... ls) {
    return make_array(make_fmatrix_impl<F, Vs...>(
        std::index_sequence<Is..., Js>{}, ls...)...);
}

template <typename F, typename... Vs>
constexpr auto make_fmatrix() {
    return make_fmatrix_impl<F, Vs...>(
        std::index_sequence<>{},
        std::make_index_sequence<variant_size<std::decay_t<Vs>>::value>{}...);
}

template <typename T>
constexpr T&& at_impl(T&& elem) {
    return std::forward<T>(elem);
}

template <typename T, typename... Is>
constexpr auto&& at_impl(T&& elems, size_t i, Is... is) {
    return at_impl(std::forward<T>(elems)[i], is...);
}

template <typename T, typename... Is>
constexpr auto&& at(T&& elems, Is... is) {
    return at_impl(std::forward<T>(elems), is...);
}

template <typename F, typename... Vs>
decltype(auto) Visit(F&& f, Vs&&... vs) {
    static constexpr auto fmatrix = make_fmatrix<F&&, Vs&&...>();
    return at(fmatrix, vs.index()...)(std::forward<F>(f),
                                      std::forward<Vs>(vs)...);
}
