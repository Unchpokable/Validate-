#include <gtest/gtest.h>
#include <core/vd_not_null.hxx>

#include <compare>
#include <memory>

// ── Test types ─────────────────────────────────────────────────────────────────

struct Widget {
    int value;
    explicit Widget(int v) noexcept : value(v) {}
    int get_value() const noexcept { return value; }
};

struct Shape {
    virtual ~Shape() = default;
    virtual int kind() const { return 0; }
};

struct Circle : Shape {
    int kind() const override { return 1; }
};

template<typename T>
struct Handle {
    T* raw;
    explicit Handle(T* p) noexcept : raw(p) {}
    T* get() const noexcept { return raw; }
};

// ── Construction ──────────────────────────────────────────────────────────────

TEST(NotNullConstruction, FromValidRawPointer)
{
    Widget w{42};
    vd::not_null<Widget*> nn(&w);
    EXPECT_EQ(nn.get(), &w);
}

TEST(NotNullConstruction, FromConstRawPointer)
{
    const Widget w{7};
    vd::not_null<const Widget*> nn(&w);
    EXPECT_EQ(nn.get(), &w);
}

TEST(NotNullConstruction, CopyConstruction)
{
    Widget w{1};
    vd::not_null<Widget*> a(&w);
    vd::not_null<Widget*> b(a);
    EXPECT_EQ(b.get(), &w);
}

TEST(NotNullConstruction, MoveConstruction)
{
    Widget w{2};
    vd::not_null<Widget*> a(&w);
    vd::not_null<Widget*> b(std::move(a));
    EXPECT_EQ(b.get(), &w);
}

// ── Access operators ──────────────────────────────────────────────────────────

TEST(NotNullAccess, DereferenceOperator)
{
    Widget w{99};
    vd::not_null<Widget*> nn(&w);
    EXPECT_EQ((*nn).value, 99);
}

TEST(NotNullAccess, ArrowOperator)
{
    Widget w{55};
    vd::not_null<Widget*> nn(&w);
    EXPECT_EQ(nn->value, 55);
    EXPECT_EQ(nn->get_value(), 55);
}

TEST(NotNullAccess, GetReturnsStoredPointer)
{
    Widget w{10};
    vd::not_null<Widget*> nn(&w);
    EXPECT_EQ(nn.get(), &w);
}

TEST(NotNullAccess, ImplicitConversionToRawPointer)
{
    Widget w{3};
    vd::not_null<Widget*> nn(&w);
    Widget* raw = nn;
    EXPECT_EQ(raw, &w);
}

TEST(NotNullAccess, ConversionToRawPointerPassedToFunction)
{
    Widget w{8};
    vd::not_null<Widget*> nn(&w);
    auto fn = [](Widget* p) { return p->value; };
    EXPECT_EQ(fn(nn), 8);
}

// ── Assignment ────────────────────────────────────────────────────────────────

TEST(NotNullAssignment, CopyAssignment)
{
    Widget a{1}, b{2};
    vd::not_null<Widget*> na(&a);
    vd::not_null<Widget*> nb(&b);
    nb = na;
    EXPECT_EQ(nb.get(), &a);
}

TEST(NotNullAssignment, MoveAssignment)
{
    Widget a{1}, b{2};
    vd::not_null<Widget*> na(&a);
    vd::not_null<Widget*> nb(&b);
    nb = std::move(na);
    EXPECT_EQ(nb.get(), &a);
}

TEST(NotNullAssignment, AssignValidRawPointer)
{
    Widget a{1}, b{99};
    vd::not_null<Widget*> nn(&a);
    nn = &b;
    EXPECT_EQ(nn.get(), &b);
    EXPECT_EQ(nn->value, 99);
}

// ── Smart pointer construction ────────────────────────────────────────────────

TEST(NotNullSmartPointers, ConstructFromSharedPtr)
{
    auto sp = std::make_shared<Widget>(42);
    vd::not_null<Widget*> nn(sp);
    EXPECT_EQ(nn.get(), sp.get());
    EXPECT_EQ(nn->value, 42);
}

TEST(NotNullSmartPointers, SharedPtrRefCountNotIncremented)
{
    auto sp = std::make_shared<Widget>(10);
    EXPECT_EQ(sp.use_count(), 1);
    {
        vd::not_null<Widget*> nn(sp);
        // not_null stores a raw pointer — shared ownership is not transferred
        EXPECT_EQ(sp.use_count(), 1);
    }
    EXPECT_EQ(sp.use_count(), 1);
}

TEST(NotNullSmartPointers, SharedPtrRemainsValidAfterNotNullDestroyed)
{
    auto sp = std::make_shared<Widget>(77);
    {
        vd::not_null<Widget*> nn(sp);
        EXPECT_EQ(nn->value, 77);
    }
    EXPECT_NE(sp.get(), nullptr);
    EXPECT_EQ(sp->value, 77);
}

TEST(NotNullSmartPointers, ConstructFromUniquePtr)
{
    auto up = std::make_unique<Widget>(33);
    Widget* raw = up.get();
    vd::not_null<Widget*> nn(up);
    EXPECT_EQ(nn.get(), raw);
}

TEST(NotNullSmartPointers, UniquePtrRemainsOwnerAfterNotNullConstruction)
{
    auto up = std::make_unique<Widget>(50);
    {
        vd::not_null<Widget*> nn(up);
        EXPECT_EQ(nn->value, 50);
    }
    // unique_ptr was not moved — it still owns the resource
    EXPECT_NE(up.get(), nullptr);
    EXPECT_EQ(up->value, 50);
}

TEST(NotNullSmartPointers, ConstructFromCustomHandleWithGet)
{
    Widget w{88};
    Handle<Widget> handle(&w);
    vd::not_null<Widget*> nn(handle);
    EXPECT_EQ(nn.get(), &w);
    EXPECT_EQ(nn->value, 88);
}

// ── Converting constructor ────────────────────────────────────────────────────

TEST(NotNullConversions, DerivedToBaseConversion)
{
    Circle c;
    vd::not_null<Circle*> nn_derived(&c);
    vd::not_null<Shape*> nn_base(nn_derived);
    EXPECT_EQ(nn_base.get(), static_cast<Shape*>(&c));
    EXPECT_EQ(nn_base->kind(), 1);
}

TEST(NotNullConversions, MutableToConstPointerConversion)
{
    Widget w{5};
    vd::not_null<Widget*> nn_mutable(&w);
    vd::not_null<const Widget*> nn_const(nn_mutable);
    EXPECT_EQ(nn_const.get(), &w);
    EXPECT_EQ(nn_const->value, 5);
}

// ── Comparison operators ──────────────────────────────────────────────────────

TEST(NotNullComparisons, EqualitySamePointer)
{
    Widget w{0};
    vd::not_null<Widget*> a(&w);
    vd::not_null<Widget*> b(&w);
    EXPECT_EQ(a, b);
    EXPECT_TRUE(a == b);
}

TEST(NotNullComparisons, EqualityDifferentPointers)
{
    Widget x{1}, y{2};
    vd::not_null<Widget*> a(&x);
    vd::not_null<Widget*> b(&y);
    EXPECT_NE(a, b);
    EXPECT_FALSE(a == b);
}

TEST(NotNullComparisons, SpaceshipSamePointerIsEqual)
{
    Widget w{0};
    vd::not_null<Widget*> a(&w);
    vd::not_null<Widget*> b(&w);
    EXPECT_EQ(a <=> b, std::strong_ordering::equal);
}

TEST(NotNullComparisons, SpaceshipDifferentPointersIsTotalOrder)
{
    Widget x{0}, y{0};
    vd::not_null<Widget*> a(&x);
    vd::not_null<Widget*> b(&y);

    auto cmp = a <=> b;
    EXPECT_NE(cmp, std::strong_ordering::equal);

    // Ordering must be antisymmetric
    auto cmp_rev = b <=> a;
    if(cmp == std::strong_ordering::less) {
        EXPECT_EQ(cmp_rev, std::strong_ordering::greater);
    } else {
        EXPECT_EQ(cmp_rev, std::strong_ordering::less);
    }
}

// ── Function signature contract ───────────────────────────────────────────────

static int read_widget(vd::not_null<const Widget*> nn)
{
    return nn->value;
}

static void write_widget(vd::not_null<Widget*> nn, int v)
{
    nn->value = v;
}

TEST(NotNullContract, AcceptsRawPointer)
{
    Widget w{10};
    EXPECT_EQ(read_widget(&w), 10);
}

TEST(NotNullContract, AcceptsSharedPtrWithoutRefCountBump)
{
    auto sp = std::make_shared<Widget>(20);
    EXPECT_EQ(read_widget(sp), 20);
    EXPECT_EQ(sp.use_count(), 1);
}

TEST(NotNullContract, AcceptsUniquePtrWithoutTransferringOwnership)
{
    auto up = std::make_unique<Widget>(30);
    EXPECT_EQ(read_widget(up), 30);
    EXPECT_NE(up.get(), nullptr);
}

TEST(NotNullContract, AcceptsNotNullDirectly)
{
    Widget w{40};
    vd::not_null<Widget*> nn(&w);
    EXPECT_EQ(read_widget(nn), 40);
}

TEST(NotNullContract, MutationThroughContract)
{
    Widget w{0};
    write_widget(&w, 99);
    EXPECT_EQ(w.value, 99);
}

TEST(NotNullContract, PolymorphicUsageThroughBaseContract)
{
    Circle c;
    vd::not_null<Circle*> nn(&c);
    auto fn = [](vd::not_null<Shape*> s) { return s->kind(); };
    EXPECT_EQ(fn(nn), 1);
}

// ── Runtime nullptr → std::terminate() ───────────────────────────────────────
// std::terminate() is the intentional response to a broken contract: it signals
// a fatal programming error, not a recoverable condition. EXPECT_DEATH verifies
// that the process terminates and that the diagnostic message reaches stderr.

TEST(NotNullDeathTest, RawNullptrTerminatesProcess)
{
    Widget* null_ptr = nullptr;
    EXPECT_DEATH({ vd::not_null<Widget*> nn(null_ptr); }, "not_null");
}

TEST(NotNullDeathTest, AssignRawNullptrTerminatesProcess)
{
    Widget w{1};
    vd::not_null<Widget*> nn(&w);
    Widget* null_ptr = nullptr;
    EXPECT_DEATH({ nn = null_ptr; }, "not_null");
}

TEST(NotNullDeathTest, SharedPtrNullTerminatesProcess)
{
    std::shared_ptr<Widget> null_sp;
    EXPECT_DEATH({ vd::not_null<Widget*> nn(null_sp); }, "not_null");
}

TEST(NotNullDeathTest, UniquePtrNullTerminatesProcess)
{
    std::unique_ptr<Widget> null_up;
    EXPECT_DEATH({ vd::not_null<Widget*> nn(null_up); }, "not_null");
}

TEST(NotNullDeathTest, CustomHandleNullTerminatesProcess)
{
    Handle<Widget> null_handle(nullptr);
    EXPECT_DEATH({ vd::not_null<Widget*> nn(null_handle); }, "not_null");
}
