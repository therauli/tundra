// For conditions of distribution and use, see copyright notice in license.txt

#pragma once

#include "AssetAPI.h"
#include "IAssetStorage.h"

class QNetworkReply;
class QBuffer;
class QNetworkAccessManager;

struct SearchRequest
{
    QNetworkReply* reply;
};

/// Represents a network source storage for assets.
class HttpAssetStorage : public IAssetStorage
{
Q_OBJECT

public:
    HttpAssetStorage();
    
    QString baseAddress;
    QString storageName;
    QStringList assetRefs;
    ///\todo Disallow scripts from changing this after the storage has been created. (security issue) -jj.
    QString localDir;
    
    /// If true, uploads are supported.
    bool writable;
    
    /// If true, assets in this storage are subject to live update after loading.
    bool liveUpdate;
    
    /// If true, storage has automatic discovery of new assets enabled.
    bool autoDiscoverable;
    
public slots:
    /// Specifies whether data can be uploaded to this asset storage.
    virtual bool Writable() const { return writable; }

    /// Specifies whether the assets in the storage should be subject to live update, once loaded
    virtual bool HasLiveUpdate() const { return liveUpdate; }
    
    /// Specifies whether the asset storage has automatic discovery of new assets enabled
    virtual bool AutoDiscoverable() const { return autoDiscoverable; }
    
    /// HttpAssetStorages are trusted if they point to a web server on the local system.
    /// \todo Make this bit configurable per-HttpAssetStorage.
    virtual bool Trusted() const;

    /// Returns the full URL of an asset with the name 'localName' if it were stored in this asset storage.
    virtual QString GetFullAssetURL(const QString &localName) { return GuaranteeTrailingSlash(baseAddress) + localName; }

    /// Returns the type of this storage: "HttpAssetStorage".
    virtual QString Type() const;

    /// Returns a human-readable name for this storage. This name is not used as an ID, and may be an empty string.
    virtual QString Name() const { return storageName; }

    /// Returns the address of this storage.
    virtual QString BaseURL() const { return baseAddress; }
    
    /// Returns all assetrefs currently known in this asset storage. Does not load the assets
    virtual QStringList GetAllAssetRefs() { return assetRefs; }
    
    /// Refresh http asset refs, issues webdav PROPFIND requests. AssetRefsChanged() will be emitted when complete.
    virtual void RefreshAssetRefs();

    /// Serializes this storage to a string for machine transfer.
    virtual QString SerializeToString() const;

    /// Add an assetref. Emit AssetRefsChanged() if did not exist already. Called by HttpAssetProvider
    void AddAssetRef(const QString& ref);

    /// Delete an assetref. Emit AssetRefsChanged() if found. Called by HttpAssetProvider
    void DeleteAssetRef(const QString& ref);
    
    /// Returns the local directory of this storage. Empty if not local.
    const QString& LocalDir() const { return localDir; }
    
private slots:
    void OnHttpTransferFinished(QNetworkReply *reply);

private:
    /// Perform a PROPFIND search on a path in the http storage
    void PerformSearch(QString path);

    /// Get QNetworkAccessManager from the parent provider
    QNetworkAccessManager* GetNetworkAccessManager();

    /// Ongoing network requests for querying asset refs
    std::vector<SearchRequest> searches;
};

