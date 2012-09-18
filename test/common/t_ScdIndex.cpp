/*
 * File:   t_ScdIndex.cpp
 * Author: Paolo D'Apice
 *
 * Created on September 11, 2012, 10:02 AM
 */

#include <boost/test/unit_test.hpp>

#include "ScdBuilder.h"
#include "Timer.hpp"
#include "common/ScdIndex.h"
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

namespace fs = boost::filesystem;

/* enable print to stdout */
#define DEBUG_OUTPUT 0

/// Test fixture and helper methods.
struct TestFixture {
    fs::path tmpdir;
    TestFixture() {
        tmpdir = createTempDir("scd-index");
        BOOST_REQUIRE(fs::exists(tmpdir));
    }
    /// Create a sample SCD file.
    fs::path createScd(const std::string& name,
            const unsigned size, const unsigned start = 1) const {
        std::cout << "Creating temporary SCD file with "
                  << size << " documents ..." << std::endl;
        fs::path file = tmpdir/name;
        fs::remove(file);
        BOOST_REQUIRE(not fs::exists(file));
        ScdBuilder scd(file);
        for (unsigned i = start; i < start + size; ++i) {
            scd("DOCID") << i;
            if (i % 2 == 0)
                scd("Title") << "Title T";
            else
                scd("Title") << "Title " << i;
            scd("Content") << "Content " << i;
        }
        BOOST_REQUIRE(fs::exists(file));
        return file;
    }
    /// Create a temporary file.
    fs::path tempFile(const std::string& name) const {
        fs::path file = tmpdir/name;
        fs::remove(file);
        BOOST_REQUIRE(not fs::exists(file));
        return file;
    }
private:
    fs::path createTempDir(const std::string& name) const {
        fs::path dir = fs::temp_directory_path()/name;
        fs::create_directories(dir);
        return dir;
    }
};

inline izenelib::util::UString
USTRING(const std::string& in) {
    return izenelib::util::UString(in, izenelib::util::UString::UTF_8);
}

inline uint128
DOCID(const unsigned in) {
    return str2uint(boost::lexical_cast<std::string>(in));
}

SCD_INDEX_PROPERTY_TAG(Title);
SCD_INDEX_PROPERTY_TAG(Content);

typedef scd::ScdIndex<Title> ScdIndex;

bool compare(const ScdIndex::document_type& a, const ScdIndex::document_type& b) {
    return a.offset < b.offset;
}

BOOST_FIXTURE_TEST_CASE(test_index, TestFixture) {
    const unsigned DOC_NUM = 10;
    fs::path path = createScd("test_index.scd", DOC_NUM);

    // build index
    ScdIndex* indexptr;
    BOOST_REQUIRE_NO_THROW(indexptr = ScdIndex::build(path.string()));

    // using const reference
    const ScdIndex& index = *indexptr;
    BOOST_CHECK_EQUAL(DOC_NUM, index.size());
#if DEBUG_OUTPUT
    std::cout << " * Index by DOCID: " << std::endl;
    std::copy(index.begin(), index.end(),
              std::ostream_iterator<ScdIndex::document_type>(std::cout, "\n"));

    std::cout << " * Index by Title: " << std::endl;
    std::copy(index.begin<Title>(), index.end<Title>(),
              std::ostream_iterator<ScdIndex::document_type>(std::cout, "\n"));
#endif
    // expected values
    const long offsets[] = { 0, 50, 93, 136, 179, 222, 265, 308, 351, 394 };
    const ScdIndex::docid_iterator docid_end = index.end();
    const ScdIndex::property_iterator title_end = index.end<Title>();

    // query by DOCID: hit
    for (size_t i = 1; i <= DOC_NUM; ++i) {
        const uint128 id = DOCID(i);
        const ScdIndex::docid_iterator& it = index.find(id);
        BOOST_CHECK(it != docid_end);
        BOOST_CHECK_EQUAL(offsets[i-1], it->offset);
        BOOST_CHECK_EQUAL(1, index.count(id));
    }
    // query by Title: hit
    // single value
    for (size_t i = 1; i <= DOC_NUM; i += 2) {
        const izenelib::util::UString title = USTRING("Title " + boost::lexical_cast<std::string>(i));
        const ScdIndex::property_iterator& it = index.find<Title>(title);
        BOOST_CHECK(it != title_end);
        BOOST_CHECK_EQUAL(offsets[i-1], it->offset);
        BOOST_CHECK_EQUAL(1, index.count<Title>(title));
    }
    // multiple values
    {
        const izenelib::util::UString title = USTRING("Title T");
        BOOST_CHECK_EQUAL(5, index.count<Title>(title));
        ScdIndex::property_range range = index.equal_range<Title>(title);
        // copy to vector and sort for comparison
        std::vector<ScdIndex::document_type> documents(range.first, range.second);
        std::sort(documents.begin(), documents.end(), compare);
#if DEBUG_OUTPUT
        std::cout << " * Documents with Title = '" << title << "'" << std::endl;
        std::copy(documents.begin(), documents.end(),
                  std::ostream_iterator<ScdIndex::document_type>(std::cout, "\n"));
#endif
        for (size_t i = 1, j = 0; i <= DOC_NUM; i += 2, ++j) {
            BOOST_CHECK_EQUAL(offsets[i], documents[j].offset);
        }
    }

    // query by DOCID: miss
    BOOST_CHECK(docid_end == index.find(DOCID(0)));
    BOOST_CHECK(docid_end == index.find(DOCID(11)));
    // query by Title: miss
    BOOST_CHECK(title_end == index.find<Title>(USTRING("Title 0")));
    BOOST_CHECK(title_end == index.find<Title>(USTRING("Title 11")));

    delete indexptr;
}

BOOST_FIXTURE_TEST_CASE(test_serialization, TestFixture) {
    const unsigned NUM_DOC_A = 10;
    const unsigned NUM_DOC_B = 8;
    fs::path path_a = createScd("test_serialization_a.scd", NUM_DOC_A);
    fs::path path_b = createScd("test_serialization_b.scd", NUM_DOC_B, NUM_DOC_A + 1);

    ScdIndex* index = ScdIndex::build(path_a.string());

    // save to file
    fs::path saved = tempFile("saved-a");
    index->save(saved.string());
    BOOST_CHECK(fs::exists(saved));

    // load from file
    {
        ScdIndex loaded;
        loaded.load(saved.string());
        BOOST_CHECK_EQUAL(index->size(), loaded.size());
        for (ScdIndex::docid_iterator it = index->begin(); it != index->end(); ++it) {
            BOOST_CHECK(*it == *loaded.find(it->docid));
        }
    }

    // version mismatch
    {
        scd::ScdIndex<Content> loaded;
        //std::cout << "content version: " << scd::ScdIndex<Content>::VERSION << std::endl;
        //std::cout << "Title   version: " << ScdIndex::VERSION << std::endl;
        BOOST_CHECK_THROW(loaded.load(saved.string()), std::exception);
    }

    // loading from file _replaces_ existing content.
    {
#if DEBUG_OUTPUT
        std::cout << " * Before load:" << std::endl;
        std::copy(index->begin(), index->end(),
                  std::ostream_iterator<ScdIndex::document_type>(std::cout, "\n"));
#endif

        ScdIndex* index_b = ScdIndex::build(path_b.string());
        fs::path saved_b = tempFile("saved-b");
        index_b->save(saved_b.string());

        index->load(saved_b.string());
#if DEBUG_OUTPUT
        std::cout << " * After load:" << std::endl;
        std::copy(index->begin(), index->end(),
                  std::ostream_iterator<ScdIndex::document_type>(std::cout, "\n"));
#endif
        BOOST_CHECK_EQUAL(index->size(), index_b->size());
        for (ScdIndex::docid_iterator it = index->begin(); it != index->end(); ++it) {
            BOOST_CHECK(*it == *index_b->find(it->docid));
        }
        delete index_b;
    }

    delete index;
}

BOOST_AUTO_TEST_CASE(test_uint) {
    { // docid as izenelib::util::UString
        const izenelib::util::UString docid = USTRING("f1667c41c7028665c198066b786bd149");

        const uint128 packed = str2uint(docid);
        const izenelib::util::UString unpacked = USTRING(uint2str(packed));
#if DEBUG_OUTPUT
        std::cout << "docid : " << docid << " -> " << sizeof(docid) << std::endl;
        std::cout << "pack  : " << packed << " -> " << sizeof(packed) << std::endl;
        std::cout << "unpack: " << unpacked << " -> " << sizeof(docid) << std::endl;
#endif
        BOOST_CHECK_EQUAL(docid, unpacked);
    }
    { // docid as std::string
        const std::string docid = "f1667c41c7028665c198066b786bd149";

        const uint128 packed = str2uint(docid);
        const std::string unpacked = uint2str(packed);
#if DEBUG_OUTPUT
        std::cout << "docid : " << docid << " -> " << sizeof(docid) << std::endl;
        std::cout << "pack  : " << packed << " -> " << sizeof(packed) << std::endl;
        std::cout << "unpack: " << unpacked << " -> " << sizeof(docid) << std::endl;
#endif
        BOOST_CHECK_EQUAL(docid, unpacked);
    }
 }

/* enable performance test on a big SCD */
#define TEST_SCD_INDEX_PERFORMANCE 0

#if TEST_SCD_INDEX_PERFORMANCE
namespace test {
SCD_INDEX_PROPERTY_TAG(DOCID);
SCD_INDEX_PROPERTY_TAG_TYPED(uuid, uint128, str2uint);
}
BOOST_FIXTURE_TEST_CASE(test_performance, TestFixture) {
#if 1
    fs::path path = createScd("test_performance.scd", 21e5);
#else
    typedef scd::ScdIndex<test::uuid, scd::DOCID> ScdIndex;
    fs::path path = "/home/paolo/tmp/B-00-201207282137-29781-U-C.SCD";
    BOOST_REQUIRE(fs::exists(path));
#endif

    Timer timer;

    timer.tic();
    ScdIndex* index = ScdIndex::build(path.string(), 2.5e4);
    timer.toc();
    std::cout << "Indexing time: " << timer.seconds() << " seconds\n";

    // save to file
    fs::path saved = tempFile("saved");
    timer.tic();
    index->save(saved.string());
    timer.toc();
    std::cout << "Serialization time: " << timer.seconds() << " seconds\n";
    BOOST_CHECK(fs::exists(saved));
    std::cout << "Archive size: " << fs::file_size(saved)/1024 << " KiB\n";
    // load from file
    {
        ScdIndex loaded;
        timer.tic();
        loaded.load(saved.string());
        timer.toc();
        std::cout << "Deserialization time: " << timer.seconds() << " seconds\n";
        BOOST_CHECK_EQUAL(index->size(), loaded.size());
        timer.tic();
        for (ScdIndex::docid_iterator it = index->begin(); it != index->end(); ++it) {
            BOOST_CHECK(*it == *loaded.find(it->docid));
        }
        timer.toc();
        std::cout << "Checking time: " << timer.seconds() << " seconds\n";
    }

    delete index;
}
#endif
