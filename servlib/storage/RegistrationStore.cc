#include "reg/Registration.h"
#include "reg/RegistrationTable.h"
#include "RegistrationStore.h"
#include "PersistentStore.h"

RegistrationStore* RegistrationStore::instance_;

/**
 * Constructor
 */
RegistrationStore::RegistrationStore(PersistentStore * store)
    : Logger("/storage/reg")
{
    store_ = store;
}

bool RegistrationStore::load()
{
#ifdef __REG_STORE_ENABLED__
    log_debug("Loading existing registrations from database.");
    // load existing stored registrations
    std::vector<int> ids;
    std::vector<int>::iterator iter;

    store_->keys(&ids);

    for (iter = ids.begin();
         iter != ids.end();
         ++iter)
    {
        int id = *iter;
        Registration * reg = new Registration(id);
        
        if (store_->get(reg, id))
        {
            if (!RegistrationTable::instance()->add(reg))
            {
                log_crit("unexpected error loading registrations");
            }
        }
    }
#endif /* __REG_STORE_ENABLED__ */
    return true;
}

/**
 * Destructor
 */
RegistrationStore::~RegistrationStore()
{
    NOTREACHED;
}

/**
 * Load in the whole database of registrations, populating the
 * given list.
 */
void
RegistrationStore::load(RegistrationList* reg_list)
{
#ifdef __REG_STORE_ENABLED__ 
    std::vector<int> ids;
    std::vector<int>::iterator iter;
    store_->keys(&ids);

    for (iter = ids.begin();
         iter != ids.end();
         ++iter)
    {
        int id = *iter;
        Registration * reg = new Registration(id);
        if (store_->get(reg, id) == 0)
        {
            reg_list->push_back(reg);
        }
    }
#endif /* __REG_STORE_ENABLED__ */
    
}

/**
 * Add a new registration to the database. Returns true if the
 * registration is successfully added, false on error.
 */
bool
RegistrationStore::add(Registration* reg)
{
#ifdef __REG_STORE_ENABLED__ 
    // ignore reserved registration ids
    if (reg->regid() < 10)
    {
        return true;
    }
    else
    {   
        return store_->add(reg, reg->regid()) == 0;
    }
#else /* __REG_STORE_ENABLED__ */
    return true;
#endif /* __REG_STORE_ENABLED__ */
}

/**
 * Remove the registration from the database, returns true if
 * successful, false on error.
 */
bool
RegistrationStore::del(Registration* reg)
{
#ifdef __REG_STORE_ENABLED__
    return store_->del(reg->regid()) == 0;
#else /* __REG_STORE_ENABLED__ */
    return true;
#endif
}
    
/**
 * Update the registration in the database. Returns true on
 * success, false if there's no matching registration or on error.
 */
bool
RegistrationStore::update(Registration* reg)
{
#ifdef __REG_STORE_ENABLED__ 
    return store_->update(reg, reg->regid()) == 0;
#else /* __REG_STORE_ENABLED__ */
    return true;
#endif /* __REG_STORE_ENABLED__ */
}

