#include "WorkerHelper.h"

#include <util/get.h>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>

namespace sf1r
{
static void buildWANDQueryTree(QueryTreePtr& rawQueryTree)
{
    if (NULL == rawQueryTree)
        return;
    if (rawQueryTree->type_ == QueryTree::KEYWORD)
        return;
    if (QueryTree::AND == rawQueryTree->type_)
        rawQueryTree->type_ = QueryTree::WAND;
    QTIter it = rawQueryTree->children_.begin();
    for (; it != rawQueryTree->children_.end(); ++it)
    {
        buildWANDQueryTree((*it));
    }
}

static void buildWANDQueryTree(QueryTreePtr& rawQueryTree, QueryTreePtr& analyzedQueryTree)
{
    if (NULL == rawQueryTree || NULL == analyzedQueryTree)
        return;
    if (QueryTree::KEYWORD == rawQueryTree->type_)
    {
        if (analyzedQueryTree->children_.begin() != analyzedQueryTree->children_.end())
            analyzedQueryTree->type_ = QueryTree::WAND;
        return;
    }
    if (QueryTree::WAND == rawQueryTree->type_)
    {
        analyzedQueryTree->type_ = QueryTree::WAND;
        QTIter itRaw = rawQueryTree->children_.begin();
        QTIter itAna = analyzedQueryTree->children_.begin();
        for (; (itRaw != rawQueryTree->children_.end()) 
             && (itAna != analyzedQueryTree->children_.end()); ++itRaw, ++itAna)
        {
            buildWANDQueryTree((*itRaw), (*itAna));
        }
    }
}

bool buildQueryTree(SearchKeywordOperation& action, IndexBundleConfiguration& bundleConfig, std::string& btqError,  PersonalSearchInfo& personalSearchInfo)
{
    action.clear();
    KeywordSearchActionItem actionItem = action.actionItem_;

    // Build raw Query Tree
    UString::EncodingType encodingType =
        UString::convertEncodingTypeFromStringToEnum(
            action.actionItem_.env_.encodingType_.c_str() );
    UString queryUStr(action.actionItem_.env_.queryString_, encodingType);//xxx
    
    if ( !action.queryParser_.parseQuery( queryUStr, action.rawQueryTree_, action.unigramFlag_, action.hasUnigramProperty_ ) )
        return false;

    if(action.rawQueryTree_->type_ == QueryTree::UNIGRAM_WILDCARD
            || action.rawQueryTree_->type_ == QueryTree::TRIE_WILDCARD
            || action.rawQueryTree_->type_ == QueryTree::EXACT
            || action.rawQueryTree_->type_ == QueryTree::ORDER
            || action.rawQueryTree_->type_ == QueryTree::NEARBY)
        action.isPhraseOrWildcardQuery_ = true;

    //queryUStr.convertString(action.actionItem_.env_.queryString_, encodingType);
    if (action.actionItem_.searchingMode_.mode_ == SearchingMode::WAND) 
    {
        buildWANDQueryTree(action.rawQueryTree_);
    }
    action.rawQueryTree_->print();

    // Build property query map
    bool applyLA = action.actionItem_.languageAnalyzerInfo_.applyLA_;

    std::vector<std::string>::const_iterator propertyIter = action.actionItem_.searchPropertyList_.begin();
    for (; propertyIter != action.actionItem_.searchPropertyList_.end(); propertyIter++)
    {
        // If there's already processed query, skip processing of this property..
        if ( action.queryTreeMap_.find( *propertyIter ) != action.queryTreeMap_.end()
                && action.propertyTermInfo_.find( *propertyIter ) != action.propertyTermInfo_.end() )
            continue;
        //std::cout << "----------------->  processing query for Property : " << *propertyIter << std::endl;

        QueryTreePtr tmpQueryTree;
        if ( applyLA )
        {
            AnalysisInfo analysisInfo;
            std::string analysis, language;
            bundleConfig.getAnalysisInfo( *propertyIter, analysisInfo, analysis, language );

            PropertyConfig propertyConfig;
            propertyConfig.propertyName_ = *propertyIter;
            IndexBundleSchema::const_iterator it = bundleConfig.indexSchema_.find(propertyConfig);

            if(!bundleConfig.searchAnalyzer_.empty())
            {
                analysisInfo.analyzerId_ = bundleConfig.searchAnalyzer_;
            }
            if (it != bundleConfig.indexSchema_.end() &&
                it->isIndex() && it->getIsFilter() && !it->isAnalyzed())
            {
                analysisInfo.analyzerId_ = "la_sia";
                analysisInfo.tokenizerNameList_.insert("tok_divide");
                analysisInfo.tokenizerNameList_.insert("tok_unite");
            }

            bool isUnigramSearchMode = action.isUnigramSearchMode_;
            if ((*propertyIter).size() > 8 && (*propertyIter).rfind("_unigram") == ((*propertyIter).size()-8))
            {
                // only unigram terms indexed for property with unigram Alias, so
                // it will fail if use word segments as rank terms in unigram searching mode.
                isUnigramSearchMode = false;
            }

            action.actionItem_.env_.expandedQueryString_.clear();
            if ( !action.queryParser_.getAnalyzedQueryTree(
                        action.actionItem_.languageAnalyzerInfo_.synonymExtension_,
                        analysisInfo, queryUStr, tmpQueryTree, action.actionItem_.env_.expandedQueryString_,
                        action.unigramFlag_, action.hasUnigramProperty_, isUnigramSearchMode, personalSearchInfo))
                return false;

        } // end - if
        else // store raw query's info into it.
            tmpQueryTree = action.rawQueryTree_;

        if (action.actionItem_.searchingMode_.mode_ == SearchingMode::WAND) 
        {
            buildWANDQueryTree(action.rawQueryTree_, tmpQueryTree);
        }
        //debug lelewang
        tmpQueryTree->print();
        action.queryTreeMap_.insert( std::make_pair(*propertyIter,tmpQueryTree) );
        PropertyTermInfo ptInfo;
        tmpQueryTree->getPropertyTermInfo(ptInfo);
        action.propertyTermInfo_.insert( std::make_pair(*propertyIter,ptInfo) );

    } // end - for
    return true;
} // end - buildQueryTree()
}
