///
/// @file CartManagerTestFixture.h
/// @brief fixture class to test CartManager
/// @author Jun Jiang
/// @date Created 2012-01-31
///

#ifndef CART_MANAGER_TEST_FIXTURE
#define CART_MANAGER_TEST_FIXTURE

#include <recommend-manager/RecTypes.h>

#include <map>
#include <string>
#include <vector>

namespace sf1r
{
class CartManager;

class CartManagerTestFixture
{
public:
    CartManagerTestFixture();

    void setCartManager(CartManager* cartManager);

    void updateCart(const std::string& userId, const std::string& items);

    void updateRandItem(const std::string& userId, int itemCount);

    void checkCartManager() const;

private:
    CartManager* cartManager_;

    typedef std::map<std::string, std::vector<itemid_t> > CartMap;
    CartMap cartMap_;
};

} // namespace sf1r

#endif //CART_MANAGER_TEST_FIXTURE
