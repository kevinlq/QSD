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

#ifndef QBS_LANGUAGE_H
#define QBS_LANGUAGE_H

#include "filetags.h"
#include "forward_decls.h"
#include "jsimports.h"

#include <buildgraph/forward_decls.h>
#include <tools/codelocation.h>
#include <tools/filetime.h>
#include <tools/set.h>
#include <tools/weakpointer.h>

#include <QtCore/qdatastream.h>
#include <QtCore/qhash.h>
#include <QtCore/qprocess.h>
#include <QtCore/qregexp.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>

#include <QtScript/qscriptvalue.h>

#include <memory>
#include <mutex>
#include <vector>

QT_BEGIN_NAMESPACE
class QScriptEngine;
QT_END_NAMESPACE

namespace qbs {
namespace Internal {
class BuildGraphLocker;
class BuildGraphLoader;
class BuildGraphVisitor;

class FileTagger
{
public:
    static FileTaggerPtr create() { return FileTaggerPtr(new FileTagger); }
    static FileTaggerPtr create(const QStringList &patterns, const FileTags &fileTags,
                                int priority) {
        return FileTaggerPtr(new FileTagger(patterns, fileTags, priority));
    }

    const QList<QRegExp> &patterns() const { return m_patterns; }
    const FileTags &fileTags() const { return m_fileTags; }
    int priority() const { return m_priority; }

    void load(PersistentPool &);
    void store(PersistentPool &) const;

private:
    FileTagger(const QStringList &patterns, const FileTags &fileTags, int priority);
    FileTagger() {}

    void setPatterns(const QStringList &patterns);

    QList<QRegExp> m_patterns;
    FileTags m_fileTags;
    int m_priority = 0;
};

class Probe
{
public:
    static ProbePtr create() { return ProbePtr(new Probe); }
    static ProbeConstPtr create(const QString &globalId,
                                const CodeLocation &location,
                                bool condition,
                                const QString &configureScript,
                                const QVariantMap &properties,
                                const QVariantMap &initialProperties,
                                const std::vector<QString> &importedFilesUsed)
    {
        return ProbeConstPtr(new Probe(globalId, location, condition, configureScript, properties,
                                       initialProperties, importedFilesUsed));
    }

    const QString &globalId() const { return m_globalId; }
    bool condition() const { return m_condition; }
    const QString &configureScript() const { return m_configureScript; }
    const QVariantMap &properties() const { return m_properties; }
    const QVariantMap &initialProperties() const { return m_initialProperties; }
    const std::vector<QString> &importedFilesUsed() const { return m_importedFilesUsed; }
    bool needsReconfigure(const FileTime &referenceTime) const;

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;

private:
    Probe() {}
    Probe(const QString &globalId,
          const CodeLocation &location,
          bool condition,
          const QString &configureScript,
          const QVariantMap &properties,
          const QVariantMap &initialProperties,
          const std::vector<QString> &importedFilesUsed)
        : m_globalId(globalId)
        , m_location(location)
        , m_configureScript(configureScript)
        , m_properties(properties)
        , m_initialProperties(initialProperties)
        , m_importedFilesUsed(importedFilesUsed)
        , m_condition(condition)
    {}

    QString m_globalId;
    CodeLocation m_location;
    QString m_configureScript;
    QVariantMap m_properties;
    QVariantMap m_initialProperties;
    std::vector<QString> m_importedFilesUsed;
    bool m_condition;
};

class RuleArtifact
{
public:
    static RuleArtifactPtr create() { return RuleArtifactPtr(new RuleArtifact); }

    QString filePath;
    FileTags fileTags;
    bool alwaysUpdated;
    CodeLocation location;
    CodeLocation filePathLocation;

    class Binding
    {
    public:
        QStringList name;
        QString code;
        CodeLocation location;

        void store(PersistentPool &pool) const;
        void load(PersistentPool &pool);

        bool operator<(const Binding &other) const
        {
            if (name == other.name) {
                if (code == other.code)
                    return location < other.location;
                return code < other.code;
            }
            return name < other.name;
        }
    };

    std::vector<Binding> bindings;

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;

private:
    RuleArtifact()
        : alwaysUpdated(true)
    {}
};
uint qHash(const RuleArtifact::Binding &b);
bool operator==(const RuleArtifact::Binding &b1, const RuleArtifact::Binding &b2);
inline bool operator!=(const RuleArtifact::Binding &b1, const RuleArtifact::Binding &b2) {
    return !(b1 == b2);
}
bool operator==(const RuleArtifact &a1, const RuleArtifact &a2);
inline bool operator!=(const RuleArtifact &a1, const RuleArtifact &a2) { return !(a1 == a2); }

class SourceArtifactInternal
{
public:
    static SourceArtifactPtr create() { return SourceArtifactPtr(new SourceArtifactInternal); }

    bool isTargetOfModule() const { return !targetOfModule.isEmpty(); }

    QString absoluteFilePath;
    FileTags fileTags;
    bool overrideFileTags;
    QString targetOfModule;
    PropertyMapPtr properties;

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;

private:
    SourceArtifactInternal() : overrideFileTags(true) {}
};
bool operator==(const SourceArtifactInternal &sa1, const SourceArtifactInternal &sa2);
inline bool operator!=(const SourceArtifactInternal &sa1, const SourceArtifactInternal &sa2) {
    return !(sa1 == sa2);
}

bool sourceArtifactSetsAreEqual(const QList<SourceArtifactPtr> &l1,
                                 const QList<SourceArtifactPtr> &l2);

class SourceWildCards
{
public:
    Set<QString> expandPatterns(const GroupConstPtr &group, const QString &baseDir,
                                 const QString &buildDir);

    const ResolvedGroup *group = nullptr;       // The owning group.
    QStringList patterns;
    QStringList excludePatterns;
    std::vector<std::pair<QString, FileTime>> dirTimeStamps;
    QList<SourceArtifactPtr> files;

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;

private:
    Set<QString> expandPatterns(const GroupConstPtr &group, const QStringList &patterns,
                                 const QString &baseDir, const QString &buildDir);
    void expandPatterns(Set<QString> &result, const GroupConstPtr &group,
                        const QStringList &parts, const QString &baseDir,
                        const QString &buildDir);
};

class QBS_AUTOTEST_EXPORT ResolvedGroup
{
public:
    static GroupPtr create() { return GroupPtr(new ResolvedGroup); }

    CodeLocation location;

    QString name;
    bool enabled;
    QString prefix;
    QList<SourceArtifactPtr> files;
    std::unique_ptr<SourceWildCards> wildcards;
    PropertyMapPtr properties;
    FileTags fileTags;
    QString targetOfModule;
    bool overrideTags;

    QList<SourceArtifactPtr> allFiles() const;

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;

private:
    ResolvedGroup()
        : enabled(true)
    {}
};

class ScriptFunction
{
public:
    static ScriptFunctionPtr create() { return ScriptFunctionPtr(new ScriptFunction); }

    ~ScriptFunction();

    QString sourceCode;
    CodeLocation location;
    ResolvedFileContextConstPtr fileContext;

    bool isValid() const;

    void load(PersistentPool &);
    void store(PersistentPool &) const;

private:
    ScriptFunction();
};

bool operator==(const ScriptFunction &a, const ScriptFunction &b);
inline bool operator!=(const ScriptFunction &a, const ScriptFunction &b) { return !(a == b); }

bool operator==(const PrivateScriptFunction &a, const PrivateScriptFunction &b);

class PrivateScriptFunction
{
    friend bool operator==(const PrivateScriptFunction &a, const PrivateScriptFunction &b);
public:
    void initialize(const ScriptFunctionPtr &sharedData) { m_sharedData = sharedData; }
    mutable QScriptValue scriptFunction; // not stored

    void load(PersistentPool &pool) { pool.load(m_sharedData); }
    void store(PersistentPool &pool) const { pool.store(m_sharedData); }

    QString &sourceCode() const { return m_sharedData->sourceCode; }
    CodeLocation &location()  const { return m_sharedData->location; }
    ResolvedFileContextConstPtr &fileContext() const { return m_sharedData->fileContext; }
    bool isValid() const { return m_sharedData->isValid(); }

private:
    ScriptFunctionPtr m_sharedData;
};

bool operator==(const PrivateScriptFunction &a, const PrivateScriptFunction &b);
inline bool operator!=(const PrivateScriptFunction &a, const PrivateScriptFunction &b)
{
    return !(a == b);
}

class ResolvedModule
{
public:
    static ResolvedModulePtr create() { return ResolvedModulePtr(new ResolvedModule); }

    QString name;
    QStringList moduleDependencies;
    PrivateScriptFunction setupBuildEnvironmentScript;
    PrivateScriptFunction setupRunEnvironmentScript;
    ResolvedProduct *product = nullptr;
    bool isProduct;

    static QStringList argumentNamesForSetupBuildEnv();
    static QStringList argumentNamesForSetupRunEnv();

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;

private:
    ResolvedModule() {}
};
bool operator==(const ResolvedModule &m1, const ResolvedModule &m2);
inline bool operator!=(const ResolvedModule &m1, const ResolvedModule &m2) { return !(m1 == m2); }

/**
  * Per default each rule is a "non-multiplex rule".
  *
  * A "multiplex rule" creates one transformer that takes all
  * input artifacts with the matching input file tag and creates
  * one or more artifacts. (e.g. linker rule)
  *
  * A "non-multiplex rule" creates one transformer per matching input file.
  */
class Rule
{
public:
    static RulePtr create() { return RulePtr(new Rule); }
    RulePtr clone() const;

    ResolvedProduct *product = nullptr;         // The owning product.
    ResolvedModuleConstPtr module;
    QString name;
    PrivateScriptFunction prepareScript;
    FileTags outputFileTags;                    // unused, if artifacts is non-empty
    PrivateScriptFunction outputArtifactsScript;    // unused, if artifacts is non-empty
    FileTags inputs;
    FileTags auxiliaryInputs;
    FileTags excludedAuxiliaryInputs;
    FileTags inputsFromDependencies;
    FileTags explicitlyDependsOn;
    bool multiplex;
    bool requiresInputs;
    QList<RuleArtifactPtr> artifacts;           // unused, if outputFileTags/outputArtifactsScript is non-empty
    bool alwaysRun;

    // members that we don't need to save
    int ruleGraphId;

    static QStringList argumentNamesForOutputArtifacts();
    static QStringList argumentNamesForPrepare();

    QString toString() const;
    bool acceptsAsInput(Artifact *artifact) const;
    FileTags staticOutputFileTags() const;
    FileTags collectedOutputFileTags() const;
    bool isDynamic() const;
    bool declaresInputs() const;

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
private:
    Rule() : multiplex(false), alwaysRun(false), ruleGraphId(-1) {}
};
bool operator==(const Rule &r1, const Rule &r2);
inline bool operator!=(const Rule &r1, const Rule &r2) { return !(r1 == r2); }
bool ruleListsAreEqual(const QList<RulePtr> &l1, const QList<RulePtr> &l2);

class ResolvedScanner
{
public:
    static ResolvedScannerPtr create() { return ResolvedScannerPtr(new ResolvedScanner); }

    ResolvedModuleConstPtr module;
    FileTags inputs;
    bool recursive;
    PrivateScriptFunction searchPathsScript;
    PrivateScriptFunction scanScript;

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;

private:
    ResolvedScanner() :
        recursive(false)
    {}
};

class TopLevelProject;
class ScriptEngine;

class QBS_AUTOTEST_EXPORT ResolvedProduct
{
public:
    static ResolvedProductPtr create() { return ResolvedProductPtr(new ResolvedProduct); }

    ~ResolvedProduct();

    bool enabled;
    FileTags fileTags;
    QString name;
    QString targetName;
    QString profile;
    QString multiplexConfigurationId;
    QString sourceDirectory;
    QString destinationDirectory;
    CodeLocation location;
    WeakPointer<ResolvedProject> project;
    QVariantMap productProperties;
    PropertyMapPtr moduleProperties;
    QList<RulePtr> rules;
    Set<ResolvedProductPtr> dependencies;
    QHash<ResolvedProductConstPtr, QVariantMap> dependencyParameters;
    QList<FileTaggerConstPtr> fileTaggers;
    QList<ResolvedModulePtr> modules;
    QHash<ResolvedModuleConstPtr, QVariantMap> moduleParameters;
    QList<ResolvedScannerConstPtr> scanners;
    QList<GroupPtr> groups;
    QList<ProbeConstPtr> probes;
    QList<ArtifactPropertiesPtr> artifactProperties;
    QStringList missingSourceFiles;
    std::unique_ptr<ProductBuildData> buildData;

    QProcessEnvironment buildEnvironment; // must not be saved
    QProcessEnvironment runEnvironment; // must not be saved

    void accept(BuildGraphVisitor *visitor) const;
    QList<SourceArtifactPtr> allFiles() const;
    QList<SourceArtifactPtr> allEnabledFiles() const;
    FileTags fileTagsForFileName(const QString &fileName) const;

    void registerArtifactWithChangedInputs(Artifact *artifact);
    void unregisterArtifactWithChangedInputs(Artifact *artifact);
    void unmarkForReapplication(const RuleConstPtr &rule);
    bool isMarkedForReapplication(const RuleConstPtr &rule) const;
    ArtifactSet lookupArtifactsByFileTag(const FileTag &tag) const;
    ArtifactSet lookupArtifactsByFileTags(const FileTags &tags) const;
    ArtifactSet targetArtifacts() const;

    TopLevelProject *topLevelProject() const;

    static QString uniqueName(const QString &name,
                              const QString &multiplexConfigurationId);
    QString uniqueName() const;
    static QString fullDisplayName(const QString &name, const QString &multiplexConfigurationId);
    QString fullDisplayName() const;

    QStringList generatedFiles(const QString &baseFile, bool recursive, const FileTags &tags) const;

    static QString deriveBuildDirectoryName(const QString &name,
                                            const QString &multiplexConfigurationId);
    QString buildDirectory() const;

    bool isInParentProject(const ResolvedProductConstPtr &other) const;
    bool builtByDefault() const;

    void cacheExecutablePath(const QString &origFilePath, const QString &fullFilePath);
    QString cachedExecutablePath(const QString &origFilePath) const;

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;

private:
    ResolvedProduct();

    QHash<QString, QString> m_executablePathCache;
    mutable std::mutex m_executablePathCacheLock;
};

class QBS_AUTOTEST_EXPORT ResolvedProject
{
public:
    virtual ~ResolvedProject();
    static ResolvedProjectPtr create() { return ResolvedProjectPtr(new ResolvedProject); }

    QString name;
    CodeLocation location;
    bool enabled;
    QList<ResolvedProductPtr> products;
    QList<ResolvedProjectPtr> subProjects;
    WeakPointer<ResolvedProject> parentProject;

    void accept(BuildGraphVisitor *visitor) const;

    void setProjectProperties(const QVariantMap &config) { m_projectProperties = config; }
    const QVariantMap &projectProperties() const { return m_projectProperties; }

    TopLevelProject *topLevelProject();
    QList<ResolvedProjectPtr> allSubProjects() const;
    QList<ResolvedProductPtr> allProducts() const;

    virtual void load(PersistentPool &pool);
    virtual void store(PersistentPool &pool) const;

protected:
    ResolvedProject();

private:
    QVariantMap m_projectProperties;
    TopLevelProject *m_topLevelProject;
};

class QBS_AUTOTEST_EXPORT TopLevelProject : public ResolvedProject
{
    friend class BuildGraphLoader;
public:
    ~TopLevelProject() override;

    static TopLevelProjectPtr create() { return TopLevelProjectPtr(new TopLevelProject); }

    static QString deriveId(const QVariantMap &config);
    static QString deriveBuildDirectory(const QString &buildRoot, const QString &id);

    QString buildDirectory; // Not saved
    QProcessEnvironment environment;
    QList<ProbeConstPtr> probes;

    QHash<QString, QString> canonicalFilePathResults; // Results of calls to "File.canonicalFilePath()."
    QHash<QString, bool> fileExistsResults; // Results of calls to "File.exists()".
    QHash<std::pair<QString, quint32>, QStringList> directoryEntriesResults; // Results of calls to "File.directoryEntries()".
    QHash<QString, FileTime> fileLastModifiedResults; // Results of calls to "File.lastModified()".
    std::unique_ptr<ProjectBuildData> buildData;
    BuildGraphLocker *bgLocker; // This holds the system-wide build graph file lock.
    bool locked; // This is the API-level lock for the project instance.

    Set<QString> buildSystemFiles;
    FileTime lastResolveTime;
    QList<ErrorInfo> warningsEncountered;

    void setBuildConfiguration(const QVariantMap &config);
    const QVariantMap &buildConfiguration() const { return m_buildConfiguration; }
    QString id() const { return m_id; }
    QString profile() const;
    QVariantMap profileConfigs;
    QVariantMap overriddenValues;

    QString buildGraphFilePath() const;
    void store(Logger logger) const;

private:
    TopLevelProject();

    void load(PersistentPool &pool) override;
    void store(PersistentPool &pool) const override;

    QString m_id;
    QVariantMap m_buildConfiguration;
};

bool artifactPropertyListsAreEqual(const QList<ArtifactPropertiesPtr> &l1,
                                   const QList<ArtifactPropertiesPtr> &l2);

QString multiplexIdToString(const QString &id);

} // namespace Internal
} // namespace qbs

QT_BEGIN_NAMESPACE
Q_DECLARE_TYPEINFO(qbs::Internal::JsImport, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(qbs::Internal::RuleArtifact::Binding, Q_MOVABLE_TYPE);
QT_END_NAMESPACE

#endif // QBS_LANGUAGE_H
