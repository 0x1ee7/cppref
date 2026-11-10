# 07 — Classes and Value Types

Scenario: a small commerce and HR toolkit. You model money, products, bank accounts, a shopping cart, employees, notification channels and semantic versions as C++ classes. The emphasis is on value semantics, invariants enforced at construction, and error reporting through `std::expected`, not on inheritance hierarchies for their own sake.

Namespace: `cppref::classes`. Your code lives in `money.hpp/.cpp`, `product.hpp/.cpp`, `accounts.hpp/.cpp`, `cart.hpp/.cpp`, `employee.hpp/.cpp`, `notifications.hpp/.cpp` and `version.hpp/.cpp`. Every function currently throws `std::logic_error("not implemented: ...")`, so every test fails until you implement it.

## Part 1 — Currency and Money construction, formatting, hashing

- `Currency::parse(string_view)`: exactly three uppercase ASCII letters, else `MoneyError::invalid_currency`. `code()`, `minor_digits()` (JPY/KRW 0, USD 2, KWD 3).
- `Money` stores an exact integer count of 10⁻⁶ units (no floating point). `Money::parse(text, currency)` accepts an optional sign, digits and an optional `.` with 1–6 fraction digits; anything else is `invalid_amount`.
- `Money::from_minor(int64, currency)`: minor units (cents); overflow is reported as `overflow`.
- `Money::zero(currency)`. There is no default constructor.
- `std::format("{}", m)` prints `10.50 USD`. Fraction digits are at least the currency's minor digits and grow as needed (`1.005 USD`, `100 JPY`). Width specs such as `{:>12}` must work.
- `operator<<`, `std::hash<Money>`; equal values hash equal.
- `==` across currencies returns `false` and never throws.

Concepts: `enum class` error codes, `std::expected`, `std::formatter` specialization, hidden friends, hash specialization.

## Part 2 — Money arithmetic, comparison, rounding, allocation

- `+`, `-` (binary and unary), `*` with `int64` on either side. Mixing currencies throws `CurrencyMismatchError` (a `std::invalid_argument`). Overflow throws `std::overflow_error`.
- Floating-point multiplication is deleted at compile time.
- `<=>` returns `std::strong_ordering` and throws on mismatched currencies.
- `is_zero()`, `is_negative()`, `is_positive()`.
- `rounded()`: round to the currency's minor digits using banker's rounding (half to even).
- `allocate(parts)` → `expected<vector<Money>, MoneyError>`: split without losing a single minor unit. The first shares absorb the remainder. `parts < 1` gives `invalid_parts`. Values that cannot be split exactly at the minor-unit level give `precision_loss`.

Concepts: operator overload design, deleted overloads, `<=>`, UB-free overflow checks, integer-only money.

## Part 3 — Product (validated value type with class-level state)

- `Product::create(sku, name, price, quantity = 0)` → `expected<Product, ProductError>`. SKU is 4–16 characters of `A-Z`, `0-9`, `-`. Name is trimmed and must be 1–100 bytes. Quantity must be ≥ 0.
- `total_created()` (static) counts successful `create()` calls only.
- `set_name` and `set_quantity` return `expected<void, ProductError>` and leave the object unchanged on error. `set_price` cannot fail.
- `restock(count)`: `count ≥ 1`, overflow-checked.
- Accessors: `sku()`, `name()`, `price()`, `quantity()`, `in_stock()`.

Concepts: static factory with validation, class-scope counter, strong exception guarantee.

## Part 4 — Account hierarchy (non-virtual interface with protected hooks)

- `Account(owner, opening_balance)`: owner is trimmed and must not be blank (`std::invalid_argument`). The copy operations are deleted.
- `deposit(amount)` and `withdraw(amount)` are public, non-virtual, and return `expected<Money, AccountError>`. `withdraw` consults the protected virtual hook `check_withdrawal` first, then the balance, then calls `on_withdrawn()`.
- `CheckingAccount(owner, balance, optional<Money> overdraft)`: the overdraft extends `available_funds()`. The overdraft must be non-negative and in the same currency.
- `SavingsAccount(owner, balance, max_withdrawals)`: only successful withdrawals count against the limit. Deposits do not count. The limit check runs before amount validation. `reset_period()` restores the allowance.

Concepts: template-method style virtual hooks, polymorphic use through `unique_ptr<Account>`, deleted copies, virtual destructor, exception-free error paths.

## Part 5 — ShoppingCart (value aggregate with a stable iteration order)

- `ShoppingCart(Currency)` (explicit). `add(product, qty = 1)` and `remove(sku, qty = 1)` return `expected<void, CartError>`.
- Lines keep insertion order in a `std::vector<CartLine>`. Each line stores its SKU, quantity and the price from the most recent `add`.
- `total()`, `size()` (total units), `empty()`, `contains(sku)` and `contains(product)`.
- `operator[](sku)` returns the quantity and throws `std::out_of_range` when the SKU is absent.
- `begin()`/`end()` give read-only iteration. Mismatched currency and foreign products are rejected.
- `operator<<` prints `ShoppingCart(currency='USD', items=3)`.

Concepts: aggregate value types, `const_iterator` exposure without leaking the container, error-returning mutators.

## Part 6 — Employee (factory from aggregate and from CSV row)

- `EmployeeFields` is a plain aggregate. `Employee::create(fields)` normalises and validates: id > 0, name and department non-blank after trimming, salary ≥ 0, valid email, skills trimmed, lowercased, de-duplicated and blank entries dropped (first occurrence wins).
- `Employee::from_csv_row(span<const string_view>)` takes 7 columns: `id, name, email, department, salary, currency, skills` (skills separated by `;`). Wrong column count is `wrong_column_count`.
- `static normalize_email(string_view)`: trims and lowercases. Exactly one `@`, non-empty local and domain parts, no whitespace.
- Accessors and `operator==` (field-wise).

Concepts: aggregate initialisation with designated initializers, `std::span` input, value equality, normalisation at the boundary.

## Part 7 — Notifications (abstract base, concrete channels, dispatcher)

- `Notifier` is an abstract base with `channel_name()` and `send(recipient, message)`; `describe()` returns `"<channel> notifier"`.
- `EmailNotifier(transport)`: the transport is a `std::function<void(string_view, string_view)>`. Invalid addresses throw `std::invalid_argument`. Transport exceptions propagate unchanged.
- `SMSNotifier(transport)`: recipient must be E.164 (`+` then 1–15 digits, no leading zero). `kMaxLength = 160`.
- `WebhookNotifier(endpoint, post)`: endpoint must start with `https://`. The body is JSON `{"message":...,"recipient":...}` with sorted keys and proper escaping. A status outside 200–299 throws `NotificationError` (a `std::runtime_error` with `status()`).
- `NotificationDispatcher(vector<unique_ptr<Notifier>>)`: rejects null or duplicate channel names. `dispatch(message, recipients)` sends to each named channel and returns one `DispatchResult{channel, exception_ptr}` per request, in request order. An unknown channel throws `std::out_of_range` before any send happens.

Concepts: abstract base class, `unique_ptr` ownership, `std::function` as a seam, `exception_ptr` for capturing failures, testing with gMock.

## Part 8 — Version (semantic versioning value type)

- `Version::create(major, minor = 0, patch = 0, prerelease = {}, build = {})` and `Version::parse(text)`. Strict form `[v]MAJOR.MINOR.PATCH[-PRE][+BUILD]`, no leading zeros in numeric fields. Errors: `invalid_component`, `invalid_prerelease`, `invalid_build`, `invalid_syntax`, `overflow`.
- Precedence follows SemVer 2.0 (numeric core, prerelease ranking, numeric identifiers compare numerically). Build metadata is ignored by `==`, `<=>` and the hash, but kept by `to_string()`.
- `bump(BumpPart::{major, minor, patch})` → `expected<Version, VersionError>`: resets lower parts and clears prerelease and build. Overflow is reported.
- `operator<<`, `std::hash<Version>`.

Concepts: `<=>` with multi-key ordering, defaulted vs custom comparison, value semantics, expected-based bumping.

## Run

From the repository root (Windows: prefix each command with `tools\vsenv.cmd`):

```
cmake --workflow --preset dev
ctest --preset dev -R "07_classes_and_value_types\.Part2"
cmake --build --preset dev --target ex07_classes_and_value_types_tests
cmake --workflow --preset solutions
```

Every test fails against the stubs. Implement a part, then run its suite (`-R "07_classes_and_value_types\.Part4"`) until it passes.

## Concepts exercised

Classes and special members; the rule of zero and five; value semantics; invariants at construction; `enum class` error codes with `std::expected`; hidden friends and `operator<=>`; `std::formatter` and `std::hash` specializations; deleted overloads; virtual destructors and template-method hooks; `unique_ptr` ownership; `std::function` seams; `std::exception_ptr`; designated aggregate initialization; `std::span`.

## Stretch ideas (untested)

- Make `Money` a `constexpr` type and add `static_assert` checks for literal arithmetic.
- Replace the `Account` hook pattern with a `std::variant` of account kinds and compare the two designs.
- Add a `Currency` registry that is safe to read from several threads.
- Serialise `Employee` to and from JSON with `std::to_chars` and `std::from_chars`.
