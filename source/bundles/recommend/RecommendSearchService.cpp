#include "RecommendSearchService.h"
#include <recommend-manager/User.h>
#include <recommend-manager/ItemCondition.h>
#include <recommend-manager/storage/UserManager.h>
#include <recommend-manager/ItemManager.h>
#include <recommend-manager/RecommenderFactory.h>
#include <recommend-manager/RecommendParam.h>
#include <recommend-manager/TIBParam.h>
#include <recommend-manager/ItemBundle.h>
#include <recommend-manager/ItemIdGenerator.h>

#include <glog/logging.h>

namespace sf1r
{

RecommendSearchService::RecommendSearchService(
    UserManager& userManager,
    ItemManager& itemManager,
    RecommenderFactory& recommenderFactory,
    ItemIdGenerator& itemIdGenerator
)
    :userManager_(userManager)
    ,itemManager_(itemManager)
    ,recommenderFactory_(recommenderFactory)
    ,itemIdGenerator_(itemIdGenerator)
{
}

bool RecommendSearchService::getUser(const std::string& userId, User& user)
{
    return userManager_.getUser(userId, user);
}

bool RecommendSearchService::recommend(
    RecommendParam& param,
    std::vector<RecommendItem>& recItemVec
)
{
    if (!convertIds_(param))
        return false;

    Recommender* recommender = recommenderFactory_.getRecommender(param.type);
    if (recommender && recommender->recommend(param, recItemVec))
        return getRecommendItems_(recItemVec);

    return false;
}

bool RecommendSearchService::convertIds_(RecommendParam& param)
{
    if (!convertItemId_(param.inputItems, param.inputItemIds) ||
        !convertItemId_(param.includeItems, param.includeItemIds) ||
        !convertItemId_(param.excludeItems, param.excludeItemIds))
    {
        return false;
    }

    return true;
}

bool RecommendSearchService::convertItemId_(
    const std::vector<std::string>& inputItemVec,
    std::vector<itemid_t>& outputItemVec
)
{
    itemid_t itemId = 0;

    for (std::size_t i = 0; i < inputItemVec.size(); ++i)
    {
        if (itemIdGenerator_.getItemIdByStrId(inputItemVec[i], itemId))
        {
            outputItemVec.push_back(itemId);
        }
    }

    return true;
}

bool RecommendSearchService::getRecommendItems_(std::vector<RecommendItem>& recItemVec) const
{
    for (std::vector<RecommendItem>::iterator it = recItemVec.begin();
        it != recItemVec.end(); ++it)
    {
        if (! itemManager_.getItem(it->item_.getId(), it->item_))
        {
            LOG(ERROR) << "error in ItemManager::getItem(), item id: " << it->item_.getId();
            return false;
        }

        std::vector<ReasonItem>& reasonItems = it->reasonItems_;
        for (std::vector<ReasonItem>::iterator reasonIt = reasonItems.begin();
            reasonIt != reasonItems.end(); ++reasonIt)
        {
            if (! itemManager_.getItem(reasonIt->item_.getId(), reasonIt->item_))
            {
                LOG(ERROR) << "error in ItemManager::getItem(), item id: " << reasonIt->item_.getId();
                return false;
            }
        }
    }

    return true;
}

bool RecommendSearchService::topItemBundle(
    const TIBParam& param,
    std::vector<ItemBundle>& bundleVec
)
{
    TIBRecommender* recommender = recommenderFactory_.getTIBRecommender();
    if (recommender && recommender->recommend(param, bundleVec))
        return getBundleItems_(bundleVec);

    return false;
}

bool RecommendSearchService::getBundleItems_(std::vector<ItemBundle>& bundleVec) const
{
    for (std::vector<ItemBundle>::iterator bundleIt = bundleVec.begin();
        bundleIt != bundleVec.end(); ++bundleIt)
    {
        std::vector<Document>& items = bundleIt->items;

        for (std::vector<Document>::iterator it = items.begin();
            it != items.end(); ++it)
        {
            if (! itemManager_.getItem(it->getId(), *it))
            {
                LOG(ERROR) << "error in ItemManager::getItem(), item id: " << it->getId();
                return false;
            }
        }
    }

    return true;
}

} // namespace sf1r
