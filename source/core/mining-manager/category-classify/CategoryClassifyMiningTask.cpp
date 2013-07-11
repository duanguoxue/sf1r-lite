#include "CategoryClassifyMiningTask.h"
#include "CategoryClassifyTable.h"
#include <document-manager/DocumentManager.h>
#include <la-manager/KNlpWrapper.h>
#include <knlp/doc_naive_bayes.h>
#include <util/ustring/UString.h>
#include <glog/logging.h>
#include <boost/filesystem/path.hpp>

using namespace sf1r;
namespace bfs = boost::filesystem;

namespace
{

void getDocPropValue(
    const Document& doc,
    const std::string& propName,
    std::string& propValue)
{
    izenelib::util::UString ustr;
    doc.getProperty(propName, ustr);
    ustr.convertString(propValue, izenelib::util::UString::UTF_8);
}

}

CategoryClassifyMiningTask::CategoryClassifyMiningTask(
    DocumentManager& documentManager,
    CategoryClassifyTable& classifyTable,
    const std::string& categoryPropName)
    : documentManager_(documentManager)
    , classifyTable_(classifyTable)
    , categoryPropName_(categoryPropName)
    , startDocId_(0)
{
}

bool CategoryClassifyMiningTask::buildDocument(docid_t docID, const Document& doc)
{
    std::string title;
    getDocPropValue(doc, classifyTable_.propName(), title);

    if (title.empty())
        return true;

    std::string classifyCategory;
    std::string category;
    if (!categoryPropName_.empty())
    {
        getDocPropValue(doc, categoryPropName_, category);
        classifyByCategory_(category, classifyCategory);
    }

    if (classifyCategory.empty())
    {
        classifyByTitle_(title, classifyCategory);
    }

    classifyTable_.setCategory(docID, classifyCategory);

    return true;
}

void CategoryClassifyMiningTask::classifyByCategory_(
    const std::string& category,
    std::string& classifyCategory)
{
    if (category.find("图书音像") != std::string::npos)
    {
        classifyCategory = "R>文娱>书籍杂志";
    }
}

void CategoryClassifyMiningTask::classifyByTitle_(
    std::string& title,
    std::string& classifyCategory)
{
    ilplib::knlp::DocNaiveBayes::makeitclean(title);

    try
    {
        KNlpWrapper* knlpWrapper = KNlpWrapper::get();
        KNlpWrapper::string_t titleKStr(title);
        KNlpWrapper::token_score_list_t tokenScores;
        knlpWrapper->fmmTokenize(titleKStr, tokenScores);

        KNlpWrapper::string_t classifyKStr = knlpWrapper->classifyToBestCategory(tokenScores);
        classifyCategory = classifyKStr.get_bytes("utf-8");
    }
    catch(std::exception& ex)
    {
        LOG(ERROR) << "exception: " << ex.what();
    }
}

bool CategoryClassifyMiningTask::preProcess()
{
    startDocId_ = classifyTable_.docIdNum();
    const docid_t endDocId = documentManager_.getMaxDocId();

    LOG(INFO) << "category classify mining task"
              << ", start docid: " << startDocId_
              << ", end docid: " << endDocId;

    if (startDocId_ > endDocId)
        return false;

    classifyTable_.resize(endDocId + 1);
    return true;
}

bool CategoryClassifyMiningTask::postProcess()
{
    if (!classifyTable_.flush())
    {
        LOG(ERROR) << "failed in CategoryClassifyTable::flush()";
        return false;
    }

    return true;
}
