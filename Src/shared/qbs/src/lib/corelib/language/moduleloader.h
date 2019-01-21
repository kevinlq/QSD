/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QBS_MODULELOADER_H
#define QBS_MODULELOADER_H

#include "filetags.h"
#include "forward_decls.h"
#include "item.h"
#include "itempool.h"
#include <logging/logger.h>
#include <tools/filetime.h>
#include <tools/set.h>
#include <tools/setupprojectparameters.h>
#include <tools/version.h>

#include <QtCore/qmap.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>

#include <map>
#include <memory>
#include <stack>
#include <unordered_map>
#include <vector>

namespace qbs {

class CodeLocation;
class Settings;

namespace Internal {

class Evaluator;
class Item;
class ItemReader;
class ProgressObserver;
class QualifiedId;

using ModulePropertiesPerGroup = std::unordered_map<const Item *, QualifiedIdSet>;

struct ModuleLoaderResult
{
    ModuleLoaderResult()
        : itemPool(new ItemPool), root(nullptr)
    {}

    struct ProductInfo
    {
        struct Dependency
        {
            FileTags productTypes;
            QString name;
            QString profile; // "*" <=> Match all profiles.
            QString multiplexConfigurationId;
            QVariantMap parameters;
            bool limitToSubProject = false;
            bool isRequired = true;

            QString uniqueName() const;
        };

        QList<ProbeConstPtr> probes;
        std::vector<Dependency> usedProducts;
        ModulePropertiesPerGroup modulePropertiesSetInGroups;
        ErrorInfo delayedError;
    };

    std::shared_ptr<ItemPool> itemPool;
    Item *root;
    QHash<Item *, ProductInfo> productInfos;
    QList<ProbeConstPtr> projectProbes;
    Set<QString> qbsFiles;
    QVariantMap profileConfigs;
};

/*
 * Loader stage II. Responsible for
 *      - loading modules and module dependencies,
 *      - project references,
 *      - Probe items.
 */
class ModuleLoader
{
public:
    ModuleLoader(Evaluator *evaluator, Logger &logger);
    ~ModuleLoader();

    void setProgressObserver(ProgressObserver *progressObserver);
    void setSearchPaths(const QStringList &searchPaths);
    void setOldProjectProbes(const QList<ProbeConstPtr> &oldProbes);
    void setOldProductProbes(const QHash<QString, QList<ProbeConstPtr>> &oldProbes);
    void setLastResolveTime(const FileTime &time) { m_lastResolveTime = time; }
    void setStoredProfiles(const QVariantMap &profiles);
    Evaluator *evaluator() const { return m_evaluator; }

    ModuleLoaderResult load(const SetupProjectParameters &parameters);

private:
    class ProductSortByDependencies;
    struct ItemCacheValue {
        explicit ItemCacheValue(Item *module = 0, bool enabled = false)
            : module(module), enabled(enabled) {}
        Item *module;
        bool enabled;
    };

    typedef QMap<std::pair<QString, QString>, ItemCacheValue> ModuleItemCache;

    class ContextBase
    {
    public:
        ContextBase()
            : item(nullptr), scope(nullptr)
        {}

        Item *item;
        Item *scope;
        QString name;
    };

    class ProjectContext;

    typedef std::vector<ModuleLoaderResult::ProductInfo::Dependency> ProductDependencies;

    class ProductContext : public ContextBase
    {
    public:
        ProjectContext *project;
        ModuleLoaderResult::ProductInfo info;
        QString profileName;
        QString multiplexConfigurationId;
        QString multiplexConfigIdForModulePrototypes;
        QVariantMap moduleProperties;
        std::map<QString, ProductDependencies> productModuleDependencies;
        std::unordered_map<const Item *, std::vector<ErrorInfo>> unknownProfilePropertyErrors;

        QString uniqueName() const;
    };

    class TopLevelProjectContext;

    class ProjectContext : public ContextBase
    {
    public:
        TopLevelProjectContext *topLevelProject;
        ModuleLoaderResult *result;
        std::vector<ProductContext> products;
        std::vector<QStringList> searchPathsStack;
    };

    struct ProductModuleInfo
    {
        Item *exportItem = nullptr;
        QString multiplexId;
        QVariantMap defaultParameters;
    };

    class TopLevelProjectContext
    {
        Q_DISABLE_COPY(TopLevelProjectContext)
    public:
        TopLevelProjectContext() {}
        ~TopLevelProjectContext() { qDeleteAll(projects); }

        std::vector<ProjectContext *> projects;
        QMultiHash<QString, ProductModuleInfo> productModules;
        QList<ProbeConstPtr> probes;
        QString buildDirectory;
    };

    class DependsContext
    {
    public:
        ProductContext *product = nullptr;
        Item *exportingProductItem = nullptr;
        ProductDependencies *productDependencies;
    };

    void handleTopLevelProject(ModuleLoaderResult *loadResult, Item *projectItem,
            const QString &buildDirectory, const Set<QString> &referencedFilePaths);
    void handleProject(ModuleLoaderResult *loadResult,
            TopLevelProjectContext *topLevelProjectContext, Item *projectItem,
            const Set<QString> &referencedFilePaths);

    typedef std::vector<VariantValuePtr> MultiplexRow;
    typedef std::vector<MultiplexRow> MultiplexTable;

    struct MultiplexInfo
    {
        std::vector<QString> properties;
        MultiplexTable table;
        bool aggregate = false;
        VariantValuePtr multiplexedType;

        QString toIdString(size_t row) const;
    };

    void dump(const MultiplexInfo &mpi);
    static MultiplexTable combine(const MultiplexTable &table, const MultiplexRow &values);
    MultiplexInfo extractMultiplexInfo(Item *productItem, Item *qbsModuleItem);
    QList<Item *> multiplexProductItem(ProductContext *dummyContext, Item *productItem);
    void adjustDependenciesForMultiplexing(const TopLevelProjectContext &tlp);
    void adjustDependenciesForMultiplexing(const ProductContext &product);

    void prepareProduct(ProjectContext *projectContext, Item *productItem);
    void setupProductDependencies(ProductContext *productContext);
    void handleProduct(ProductContext *productContext);
    void checkDependencyParameterDeclarations(const ProductContext *productContext) const;
    void handleModuleSetupError(ProductContext *productContext, const Item::Module &module,
                                const ErrorInfo &error);
    void initProductProperties(const ProductContext &product);
    void handleSubProject(ProjectContext *projectContext, Item *projectItem,
            const Set<QString> &referencedFilePaths);
    QList<Item *> loadReferencedFile(const QString &relativePath,
                                     const CodeLocation &referencingLocation,
                                     const Set<QString> &referencedFilePaths,
                                     ProductContext &dummyContext);
    void handleAllPropertyOptionsItems(Item *item);
    void handlePropertyOptions(Item *optionsItem);

    using ModuleDependencies = QHash<QualifiedId, QualifiedIdSet>;
    void setupReverseModuleDependencies(const Item::Module &module, ModuleDependencies &deps,
                                        QualifiedIdSet &seenModules);
    ModuleDependencies setupReverseModuleDependencies(const Item *product);
    void handleGroup(ProductContext *productContext, Item *groupItem,
                     const ModuleDependencies &reverseDepencencies);
    void propagateModulesFromParent(ProductContext *productContext, Item *groupItem,
                                    const ModuleDependencies &reverseDepencencies);
    void adjustDefiningItemsInGroupModuleInstances(const Item::Module &module,
                                                   const Item::Modules &dependentModules);

    void mergeExportItems(const ProductContext &productContext);
    void resolveDependencies(DependsContext *dependsContext, Item *item);
    class ItemModuleList;
    void resolveDependsItem(DependsContext *dependsContext, Item *parentItem, Item *dependsItem,
                            ItemModuleList *moduleResults, ProductDependencies *productResults);
    void forwardParameterDeclarations(const Item *dependsItem, const ItemModuleList &modules);
    void forwardParameterDeclarations(const QualifiedId &moduleName, Item *item,
                                      const ItemModuleList &modules);
    void resolveParameterDeclarations(const Item *module);
    QVariantMap extractParameters(Item *dependsItem) const;
    Item *moduleInstanceItem(Item *containerItem, const QualifiedId &moduleName);
    static ProductModuleInfo *productModule(ProductContext *productContext, const QString &name,
                                            const QString &multiplexId);
    static ProductContext *product(ProjectContext *projectContext, const QString &name);
    static ProductContext *product(TopLevelProjectContext *tlpContext, const QString &name);
    Item *loadModule(ProductContext *productContext, Item *exportingProductItem, Item *item,
            const CodeLocation &dependsItemLocation, const QString &moduleId,
            const QualifiedId &moduleName, const QString &multiplexId, bool isRequired,
                     bool *isProductDependency, QVariantMap *defaultParameters);
    Item *searchAndLoadModuleFile(ProductContext *productContext,
            const CodeLocation &dependsItemLocation, const QualifiedId &moduleName,
            bool isRequired, Item *moduleInstance);
    Item *loadModuleFile(ProductContext *productContext, const QString &fullModuleName,
            bool isBaseModule, const QString &filePath, bool *triedToLoad, Item *moduleInstance);
    Item::Module loadBaseModule(ProductContext *productContext, Item *item);
    void setupBaseModulePrototype(Item *prototype);
    void instantiateModule(ProductContext *productContext, Item *exportingProductItem,
            Item *instanceScope, Item *moduleInstance, Item *modulePrototype,
            const QualifiedId &moduleName, ProductModuleInfo *productModuleInfo);
    void createChildInstances(Item *instance, Item *prototype,
                              QHash<Item *, Item *> *prototypeInstanceMap) const;
    void resolveProbes(ProductContext *productContext, Item *item);
    void resolveProbe(ProductContext *productContext, Item *parent, Item *probe);
    void checkCancelation() const;
    bool checkItemCondition(Item *item, Item *itemToDisable = nullptr);
    QStringList readExtraSearchPaths(Item *item, bool *wasSet = 0);
    void copyProperties(const Item *sourceProject, Item *targetProject);
    Item *wrapInProjectIfNecessary(Item *item);
    static QString findExistingModulePath(const QString &searchPath,
            const QualifiedId &moduleName);
    static void setScopeForDescendants(Item *item, Item *scope);
    void overrideItemProperties(Item *item, const QString &buildConfigKey,
                                const QVariantMap &buildConfig);
    void addProductModuleDependencies(ProductContext *ctx, const Item::Module &module);
    void addTransitiveDependencies(ProductContext *ctx);
    Item *createNonPresentModule(const QString &name, const QString &reason, Item *module);
    void copyGroupsFromModuleToProduct(const ProductContext &productContext,
                                       const Item::Module &module, const Item *modulePrototype);
    void copyGroupsFromModulesToProduct(const ProductContext &productContext);
    void markModuleTargetGroups(Item *group, const Item::Module &module);
    bool checkExportItemCondition(Item *exportItem, const ProductContext &productContext);
    ProbeConstPtr findOldProjectProbe(const QString &globalId, bool condition,
                                      const QVariantMap &initialProperties,
                                      const QString &sourceCode) const;
    ProbeConstPtr findOldProductProbe(const QString &productName, bool condition,
                                      const QVariantMap &initialProperties,
                                      const QString &sourceCode) const;
    ProbeConstPtr findCurrentProbe(const CodeLocation &location, bool condition,
                                   const QVariantMap &initialProperties) const;

    enum class CompareScript { No, Yes };
    bool probeMatches(const ProbeConstPtr &probe, bool condition,
                      const QVariantMap &initialProperties, const QString &configureScript,
                      CompareScript compareScript) const;

    void printProfilingInfo();
    void handleProductError(const ErrorInfo &error, ProductContext *productContext);
    QualifiedIdSet gatherModulePropertiesSetInGroup(const Item *group);
    Item *loadItemFromFile(const QString &filePath);
    void collectProductsByName(const TopLevelProjectContext &topLevelProject);

    void handleProfileItems(Item *item, ProjectContext *projectContext);
    std::vector<Item *> collectProfileItems(Item *item, ProjectContext *projectContext);
    void evaluateProfileValues(const QualifiedId &namePrefix, Item *item, Item *profileItem,
                               QVariantMap &values);
    void handleProfile(Item *profileItem);
    void collectNameFromOverride(const QString &overrideString);
    void checkProjectNamesInOverrides(const TopLevelProjectContext &tlp);
    void checkProductNamesInOverrides();

    ItemPool *m_pool;
    Logger &m_logger;
    ProgressObserver *m_progressObserver;
    ItemReader *m_reader;
    Evaluator *m_evaluator;
    QMap<QString, QStringList> m_moduleDirListCache;
    ModuleItemCache m_modulePrototypeItemCache;
    QHash<const Item *, Item::PropertyDeclarationMap> m_parameterDeclarations;
    Set<Item *> m_disabledItems;
    std::vector<bool> m_requiredChain;

    struct DependsChainEntry
    {
        DependsChainEntry(const QualifiedId &name, const CodeLocation &location)
            : name(name), location(location)
        {
        }

        QualifiedId name;
        CodeLocation location;
        bool isProduct = false;
    };
    class DependsChainManager;
    std::vector<DependsChainEntry> m_dependsChain;

    QHash<QString, QList<ProbeConstPtr>> m_oldProjectProbes;
    QHash<QString, QList<ProbeConstPtr>> m_oldProductProbes;
    FileTime m_lastResolveTime;
    QHash<CodeLocation, QList<ProbeConstPtr>> m_currentProbes;
    QVariantMap m_storedProfiles;
    QVariantMap m_localProfiles;
    std::multimap<QString, const ProductContext *> m_productsByName;
    SetupProjectParameters m_parameters;
    std::unique_ptr<Settings> m_settings;
    Version m_qbsVersion;
    Item *m_tempScopeItem = nullptr;
    qint64 m_elapsedTimeProbes;
    Set<QString> m_projectNamesUsedInOverrides;
    Set<QString> m_productNamesUsedInOverrides;
    Set<QString> m_disabledProjects;
};

} // namespace Internal
} // namespace qbs

QT_BEGIN_NAMESPACE
Q_DECLARE_TYPEINFO(qbs::Internal::ModuleLoaderResult::ProductInfo, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(qbs::Internal::ModuleLoaderResult::ProductInfo::Dependency, Q_MOVABLE_TYPE);
QT_END_NAMESPACE

#endif // QBS_MODULELOADER_H
