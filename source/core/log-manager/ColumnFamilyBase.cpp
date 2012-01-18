#include "ColumnFamilyBase.h"

#include <libcassandra/cassandra.h>

using namespace std;
using namespace libcassandra;
using namespace org::apache::cassandra;

namespace sf1r {

ColumnFamilyBase::ColumnFamilyBase()
{
}

ColumnFamilyBase::~ColumnFamilyBase()
{
}

bool ColumnFamilyBase::truncateColumnFamily() const
{
    if (!isEnabled()) return false;
    try
    {
        CassandraConnection::instance().getCassandraClient(getKeyspaceName())->truncateColumnFamily(getName());
    }
    CATCH_CASSANDRA_EXCEPTION("[CassandraConnection] error:");

    return true;
}

bool ColumnFamilyBase::dropColumnFamily() const
{
    if (!isEnabled()) return false;
    try
    {
        CassandraConnection::instance().getCassandraClient(getKeyspaceName())->dropColumnFamily(getName());
    }
    CATCH_CASSANDRA_EXCEPTION("[CassandraConnection] error:");

    return true;
}

bool ColumnFamilyBase::getSlice(const string& start, const string& finish, int32_t count, bool reversed)
{
    if (!isEnabled() || getKey().empty()) return false;
    try
    {
        ColumnParent col_parent;
        col_parent.__set_column_family(getName());

        SlicePredicate pred;
        pred.__isset.slice_range = true;
        pred.slice_range.__set_start(start);
        pred.slice_range.__set_finish(finish);
        pred.slice_range.__set_count(count);
        pred.slice_range.__set_reversed(reversed);

        vector<ColumnOrSuperColumn> raw_column_list;
        CassandraConnection::instance().getCassandraClient(getKeyspaceName())->getRawSlice(
                raw_column_list,
                getKey(),
                col_parent,
                pred);
        if (raw_column_list.empty()) return true;

        clear();
        switch (getColumnType())
        {
        case NORMAL:
            for (vector<ColumnOrSuperColumn>::const_iterator it = raw_column_list.begin();
                    it != raw_column_list.end(); ++it)
            {
                insert(it->column.name, it->column.value);
            }
            break;
        case COUNTER:
            for (vector<ColumnOrSuperColumn>::const_iterator it = raw_column_list.begin();
                    it != raw_column_list.end(); ++it)
            {
                insertCounter(it->counter_column.name, it->counter_column.value);
            }
            break;
        default:
            break;
        }
    }
    CATCH_CASSANDRA_EXCEPTION("[CassandraConnection] error:");

    return true;
}

bool ColumnFamilyBase::deleteRow()
{
    if (!isEnabled() || getKey().empty()) return false;
    try
    {
        ColumnPath col_path;
        col_path.__set_column_family(getName());
        CassandraConnection::instance().getCassandraClient(getKeyspaceName())->remove(
                getKey(),
                col_path);
    }
    CATCH_CASSANDRA_EXCEPTION("[CassandraConnection] error:");

    return true;
}

bool ColumnFamilyBase::getCount(int32_t& count, const string& start, const string& finish) const
{
    if (!isEnabled() || getKey().empty()) return false;
    try
    {
        ColumnParent col_parent;
        col_parent.__set_column_family(getName());

        SlicePredicate pred;
        pred.__isset.slice_range = true;
        pred.slice_range.__set_start(start);
        pred.slice_range.__set_finish(finish);
        //pred.slice_range.__set_count(numeric_limits<int32_t>::max());

        count = CassandraConnection::instance().getCassandraClient(getKeyspaceName())->getCount(
                getKey(),
                col_parent,
                pred);
    }
    CATCH_CASSANDRA_EXCEPTION("[CassandraConnection] error:");

    return true;
}

}
