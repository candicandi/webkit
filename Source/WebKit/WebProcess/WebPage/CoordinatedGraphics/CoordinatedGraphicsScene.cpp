/*
    Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2012 Company 100, Inc.
    Copyright (C) 2017 Sony Interactive Entertainment Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "CoordinatedGraphicsScene.h"

#if USE(COORDINATED_GRAPHICS)
#include <WebCore/CoordinatedBackingStore.h>
#include <WebCore/CoordinatedPlatformLayerBuffer.h>
#include <WebCore/CoordinatedTileBuffer.h>
#include <WebCore/Damage.h>
#include <WebCore/NicosiaCompositionLayer.h>
#include <WebCore/NicosiaScene.h>
#include <WebCore/TextureMapperLayer.h>
#include <wtf/Atomics.h>
#include <wtf/SystemTracing.h>

namespace WebKit {
using namespace WebCore;

CoordinatedGraphicsScene::CoordinatedGraphicsScene(CoordinatedGraphicsSceneClient* client)
    : m_client(client)
{
}

CoordinatedGraphicsScene::~CoordinatedGraphicsScene() = default;

void CoordinatedGraphicsScene::applyStateChanges(const Vector<RefPtr<Nicosia::Scene>>& states)
{
    if (!m_textureMapper) {
        m_textureMapper = TextureMapper::create(TextureMapper::OpenGLMode);
        static_cast<TextureMapperGL*>(m_textureMapper.get())->setEnableEdgeDistanceAntialiasing(true);
    }

    ensureRootLayer();

    for (auto& scene : states)
        commitSceneState(scene);
}

void CoordinatedGraphicsScene::paintToCurrentGLContext(const TransformationMatrix& matrix, const FloatRect& clipRect, bool flipY)
{
    updateSceneState();

    TextureMapperLayer* currentRootLayer = rootLayer();
    if (!currentRootLayer)
        return;

    if (currentRootLayer->transform() != matrix)
        currentRootLayer->setTransform(matrix);

    bool sceneHasRunningAnimations = currentRootLayer->applyAnimationsRecursively(MonotonicTime::now());

    bool didChangeClipRect = false;
    FloatRoundedRect actualClipRect(clipRect);
#if ENABLE(DAMAGE_TRACKING)
    if (m_client && m_damagePropagation != Damage::Propagation::None) {
        Damage frameDamage;
        if (sceneHasRunningAnimations) {
            // When running animations for now we need to damage the whole frame.
            frameDamage.add(clipRect);
        } else {
            WTFBeginSignpost(this, CollectDamage);
            currentRootLayer->collectDamage(*m_textureMapper, frameDamage);
            WTFEndSignpost(this, CollectDamage);

            ASSERT(!frameDamage.isInvalid());
            if (m_damagePropagation == Damage::Propagation::Unified) {
                Damage boundsDamage;
                boundsDamage.add(frameDamage.bounds());
                frameDamage = WTFMove(boundsDamage);
            }
        }

        const auto& damageSinceLastSurfaceUse = m_client->addSurfaceDamage(frameDamage);
        if (!damageSinceLastSurfaceUse.isInvalid()) {
            actualClipRect = static_cast<FloatRoundedRect>(damageSinceLastSurfaceUse.bounds());
            didChangeClipRect = true;
        }
    }
#endif

    WTFBeginSignpost(this, PaintTextureMapperLayerTree);
    m_textureMapper->beginPainting(flipY ? TextureMapper::FlipY::Yes : TextureMapper::FlipY::No);
    m_textureMapper->beginClip(TransformationMatrix(), actualClipRect);
    currentRootLayer->paint(*m_textureMapper);
    if (!didChangeClipRect)
        m_fpsCounter.updateFPSAndDisplay(*m_textureMapper, clipRect.location(), matrix);
    m_textureMapper->endClip();
    m_textureMapper->endPainting();
    WTFEndSignpost(this, PaintTextureMapperLayerTree);

    if (didChangeClipRect && m_fpsCounter.isActive()) {
        m_textureMapper->beginPainting(flipY ? TextureMapper::FlipY::Yes : TextureMapper::FlipY::No);
        m_textureMapper->beginClip(TransformationMatrix(), FloatRoundedRect(clipRect));
        m_fpsCounter.updateFPSAndDisplay(*m_textureMapper, clipRect.location(), matrix);
        m_textureMapper->endClip();
        m_textureMapper->endPainting();
    }

    if (sceneHasRunningAnimations)
        updateViewport();
}

#if PLATFORM(QT)
void CoordinatedGraphicsScene::setScrollPosition(const FloatPoint& scrollPosition)
{
    m_scrollPosition = scrollPosition;
}
#endif

void CoordinatedGraphicsScene::updateViewport()
{
    if (m_client)
        m_client->updateViewport();
}

void CoordinatedGraphicsScene::onNewBufferAvailable()
{
    updateViewport();
}

static TextureMapperLayer& texmapLayer(Nicosia::CompositionLayer& compositionLayer)
{
    auto& compositionState = compositionLayer.compositionState();
    if (!compositionState.layer) {
        compositionState.layer = makeUnique<TextureMapperLayer>();
        compositionState.layer->setID(compositionLayer.id());
    }
    return *compositionState.layer;
}

static void updateBackingStore(TextureMapperLayer& layer, CoordinatedBackingStore& backingStore, const CoordinatedBackingStoreProxy::Update& update)
{
    layer.setBackingStore(&backingStore);
    backingStore.resize(layer.size(), update.scale());

    for (auto tileID : update.tilesToCreate())
        backingStore.createTile(tileID);
    for (auto tileID : update.tilesToRemove())
        backingStore.removeTile(tileID);
    for (const auto& tileUpdate : update.tilesToUpdate())
        backingStore.updateTile(tileUpdate.tileID, tileUpdate.dirtyRect, tileUpdate.tileRect, tileUpdate.buffer.copyRef(), { });
}

void CoordinatedGraphicsScene::removeLayer(Nicosia::CompositionLayer& layer)
{
    layer.accessCommitted([](const Nicosia::CompositionLayer::LayerState& committed) {
        if (committed.contentLayer)
            committed.contentLayer->invalidate();
    });

    auto& compositionState = layer.compositionState();
    m_backingStores.remove(compositionState.layer.get());
    compositionState.layer = nullptr;
}

void CoordinatedGraphicsScene::commitSceneState(const RefPtr<Nicosia::Scene>& scene)
{
    if (!m_client)
        return;

    m_nicosia.scene = scene;
}

void CoordinatedGraphicsScene::updateSceneState()
{
    if (!m_nicosia.scene)
        return;

    // Store layer and impl references along with the corresponding update
    // for each type of possible layer backing.
    struct {
        struct BackingStore {
            std::reference_wrapper<TextureMapperLayer> layer;
            CoordinatedBackingStoreProxy::Update update;
        };
        Vector<BackingStore> backingStore;

        struct ContentLayer {
            std::reference_wrapper<TextureMapperLayer> layer;
            std::reference_wrapper<TextureMapperPlatformLayerProxy> proxy;
            bool needsActivation { false };
        };
        Vector<ContentLayer> contentLayer;

        struct ImageBacking {
            std::reference_wrapper<TextureMapperLayer> layer;
            RefPtr<CoordinatedImageBackingStore> store;
            bool isVisible { false };
        };
        Vector<ImageBacking> imageBacking;
    } layersByBacking;
    HashSet<Ref<WebCore::TextureMapperPlatformLayerProxy>> replacedProxiesToInvalidate;

    // Access the scene state and perform state update for each layer.
    m_nicosia.scene->accessState(
        [this, &layersByBacking, &replacedProxiesToInvalidate](Nicosia::Scene::State& state)
        {
            // FIXME: try to minimize the amount of work in case the Scene::State object
            // didn't change (i.e. no layer flush was done), but don't forget to properly
            // gather and update proxy objects for content layers.

            // Handle the root layer, adding it to the TextureMapperLayer tree
            // on the first update. No such change is expected later.
            {
                auto& rootLayer = texmapLayer(*state.rootLayer);
                if (rootLayer.id() != m_rootLayerID) {
                    m_rootLayerID = rootLayer.id();
                    RELEASE_ASSERT(m_rootLayer->children().isEmpty());
                    m_rootLayer->addChild(&rootLayer);
                }
            }

            HashSet<RefPtr<Nicosia::CompositionLayer>> removedLayers;
            for (auto& layer : m_nicosia.state.layers) {
                // Gather all the to-be-removed layers so that composition-side state
                // can be properly purged after the current state's set of layers is adopted.
                if (!state.layers.contains(layer))
                    removedLayers.add(layer);
                else {
                    // Store references to all the proxies that are being used by the layers that are kept in the tree.
                    // When adopting the new state, the existent proxies may be replaced or detached from their layers, causing the
                    // reference to be lost without having a chance to invalidate them. After the call to commitState, we will
                    // invalidate all the proxies that are not being used anymore.
                    layer->accessCommitted(
                        [&replacedProxiesToInvalidate](const Nicosia::CompositionLayer::LayerState& committed)
                        {
                            if (committed.contentLayer)
                                replacedProxiesToInvalidate.add(Ref { *committed.contentLayer });
                        });
                }
            }

            m_nicosia.state = state;

            for (auto& layer : removedLayers)
                removeLayer(*layer);
            removedLayers = { };

            // Iterate the current state's set of layers, updating state values according to
            // the incoming state changes. Layer backings are stored so that the updates
            // (possibly time-consuming) can be done outside of this scene update.
            for (auto& compositionLayer : m_nicosia.state.layers) {
                auto& layer = texmapLayer(*compositionLayer);

                compositionLayer->commitState(
                    [this, &layer, &layersByBacking, &replacedProxiesToInvalidate]
                    (const Nicosia::CompositionLayer::LayerState& layerState)
                    {
                        if (layerState.delta.positionChanged)
                            layer.setPosition(layerState.position);
                        if (layerState.delta.anchorPointChanged)
                            layer.setAnchorPoint(layerState.anchorPoint);
                        if (layerState.delta.sizeChanged)
                            layer.setSize(layerState.size);
                        if (layerState.delta.boundsOriginChanged)
                            layer.setBoundsOrigin(layerState.boundsOrigin);

                        if (layerState.delta.transformChanged)
                            layer.setTransform(layerState.transform);
                        if (layerState.delta.childrenTransformChanged)
                            layer.setChildrenTransform(layerState.childrenTransform);

                        if (layerState.delta.contentsRectChanged)
                            layer.setContentsRect(layerState.contentsRect);
                        if (layerState.delta.contentsTilingChanged) {
                            layer.setContentsTilePhase(layerState.contentsTilePhase);
                            layer.setContentsTileSize(layerState.contentsTileSize);
                        }
                        if (layerState.delta.contentsClippingRectChanged)
                            layer.setContentsClippingRect(layerState.contentsClippingRect);

                        if (layerState.delta.opacityChanged)
                            layer.setOpacity(layerState.opacity);
                        if (layerState.delta.solidColorChanged)
                            layer.setSolidColor(layerState.solidColor);

                        if (layerState.delta.filtersChanged)
                            layer.setFilters(layerState.filters);
                        if (layerState.delta.backdropFiltersChanged)
                            layer.setBackdropLayer(layerState.backdropLayer ? &texmapLayer(*layerState.backdropLayer) : nullptr);
                        if (layerState.delta.backdropFiltersRectChanged)
                            layer.setBackdropFiltersRect(layerState.backdropFiltersRect);
                        if (layerState.delta.animationsChanged)
                            layer.setAnimations(layerState.animations);

                        if (layerState.delta.childrenChanged) {
                            layer.setChildren(WTF::map(layerState.children, [](auto& child) {
                                return &texmapLayer(*child);
                            }));
                        }

                        if (layerState.delta.maskChanged)
                            layer.setMaskLayer(layerState.mask ? &texmapLayer(*layerState.mask) : nullptr);
                        if (layerState.delta.replicaChanged)
                            layer.setReplicaLayer(layerState.replica ? &texmapLayer(*layerState.replica) : nullptr);

                        if (layerState.delta.flagsChanged) {
                            layer.setContentsOpaque(layerState.flags.contentsOpaque);
                            layer.setDrawsContent(layerState.flags.drawsContent);
                            layer.setContentsVisible(layerState.flags.contentsVisible);
                            layer.setBackfaceVisibility(layerState.flags.backfaceVisible);
                            layer.setMasksToBounds(layerState.flags.masksToBounds);
                            layer.setContentsRectClipsDescendants(layerState.flags.contentsRectClipsDescendants);
                            layer.setPreserves3D(layerState.flags.preserves3D);
                        }

                        if (layerState.delta.repaintCounterChanged) {
                            layer.setShowRepaintCounter(layerState.repaintCounter.visible);
                            layer.setRepaintCount(layerState.repaintCounter.count);
                        }

                        if (layerState.delta.debugBorderChanged) {
                            layer.setShowDebugBorder(layerState.debugBorder.visible);
                            layer.setDebugBorderColor(layerState.debugBorder.color);
                            layer.setDebugBorderWidth(layerState.debugBorder.width);
                        }

                        if (layerState.backingStore)
                            layersByBacking.backingStore.append({ std::ref(layer), layerState.backingStore->takePendingUpdate() });
                        else {
                            layer.setBackingStore(nullptr);
                            m_backingStores.remove(&layer);
                        }

                        if (layerState.contentLayer) {
                            layersByBacking.contentLayer.append(
                                { std::ref(layer), std::ref(*layerState.contentLayer), layerState.delta.contentLayerChanged });
                            replacedProxiesToInvalidate.remove(Ref { *layerState.contentLayer });
                        } else if (layerState.imageBacking.store) {
                            layersByBacking.imageBacking.append(
                                { std::ref(layer), layerState.imageBacking.store, layerState.imageBacking.isVisible });
                        } else
                            layer.setContentsLayer(nullptr);

                        if (layerState.animatedBackingStoreClient)
                            layer.setAnimatedBackingStoreClient(layerState.animatedBackingStoreClient.get());
                        else
                            layer.setAnimatedBackingStoreClient(nullptr);

#if ENABLE(DAMAGE_TRACKING)
                        if (layerState.delta.damageChanged)
                            layer.setDamage(layerState.damage);
#endif
                    });
            }
        });

    // Iterate through each backing type of layers and gather backing store
    // or proxy objects that need an update.
    // FIXME: HashSet<std::reference_wrapper<>> would be ideal, but doesn't work (yet).
    HashSet<Ref<WebCore::CoordinatedBackingStore>> backingStoresWithPendingBuffers;
    HashSet<Ref<WebCore::TextureMapperPlatformLayerProxy>> proxiesForSwapping;

    {
        for (auto& entry : layersByBacking.backingStore) {
            auto addResult = m_backingStores.ensure(&entry.layer.get(), [] {
                return CoordinatedBackingStore::create();
            });

            auto& backingStore = addResult.iterator->value.get();
            updateBackingStore(entry.layer.get(), backingStore, entry.update);
            backingStoresWithPendingBuffers.add(backingStore);
        }

        layersByBacking.backingStore = { };
    }

    // Invalidate the proxies that are not used anymore before activating the new ones:
    // a layer's proxy can be replaced with a new one. If we don't invalidate the old proxy
    // before activating the new one, the contentLayer set to the layer by the activation of
    // the new proxy will be removed by the invalidation of the old one.
    for (auto& proxy : replacedProxiesToInvalidate)
        proxy->invalidate();
    replacedProxiesToInvalidate = { };

    {
        for (auto& entry : layersByBacking.contentLayer) {
            auto& proxy = entry.proxy.get();
            if (entry.needsActivation)
                proxy.activateOnCompositingThread(this, &entry.layer.get());
            proxiesForSwapping.add(proxy);
        }

        layersByBacking.contentLayer = { };
    }

    {
        for (auto& entry : layersByBacking.imageBacking) {
            auto& layer = entry.layer.get();
            if (!entry.isVisible)
                layer.setContentsLayer(nullptr);
            else if (entry.store)
                layer.setContentsLayer(entry.store->buffer());
        }

        layersByBacking.imageBacking = { };
    }

    for (auto& backingStore : backingStoresWithPendingBuffers)
        backingStore->processPendingUpdates(*m_textureMapper);

    for (auto& proxy : proxiesForSwapping)
        proxy->swapBuffer();
}

void CoordinatedGraphicsScene::ensureRootLayer()
{
    if (m_rootLayer)
        return;

    m_rootLayer = makeUnique<TextureMapperLayer>();
    m_rootLayer->setMasksToBounds(false);
    m_rootLayer->setDrawsContent(false);
    m_rootLayer->setAnchorPoint(FloatPoint3D(0, 0, 0));

    // The root layer should not have zero size, or it would be optimized out.
    m_rootLayer->setSize(FloatSize(1.0, 1.0));
}

void CoordinatedGraphicsScene::purgeGLResources()
{
    ASSERT(!m_client);

    if (m_nicosia.scene) {
        m_nicosia.scene->accessState([this](Nicosia::Scene::State& state) {
            for (auto& layer : state.layers)
                removeLayer(*layer);
            state.layers = { };
            state.rootLayer = nullptr;
        });
        m_nicosia.scene = nullptr;
    }

    m_rootLayer = nullptr;
    m_rootLayerID = 0;
    m_textureMapper = nullptr;
}

void CoordinatedGraphicsScene::detach()
{
    ASSERT(RunLoop::isMain());
    m_isActive = false;
    m_client = nullptr;
}

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
