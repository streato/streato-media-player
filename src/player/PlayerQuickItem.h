#ifndef PLAYERQUICKITEM_H
#define PLAYERQUICKITEM_H

#include <Qt>
#include <QtQuick/QQuickItem>
#include <QOpenGLFramebufferObject>

#include <mpv/client.h>
#include <mpv/render.h>

#ifdef Q_OS_WIN32
#include <windows.h>
#else
typedef void* HANDLE;
#endif

#include "PlayerComponent.h"
#include "QtHelper.h"

class PlayerRenderer : public QObject
{
  Q_OBJECT
  friend class PlayerQuickItem;

  PlayerRenderer(mpv::qt::Handle mpv, QQuickWindow* window);
  bool init();
  ~PlayerRenderer() override;
  void render();
  void swap();

public slots:
  void onVideoPlaybackActive(bool active);

private:
  static void on_update(void *ctx);
  mpv::qt::Handle m_mpv;
  mpv_render_context* m_mpvGL;
  QQuickWindow* m_window;
  QSize m_size;
  HANDLE m_hAvrtHandle;
  QRect m_videoRectangle;
  QOpenGLFramebufferObject* m_fbo;
};

class PlayerQuickItem : public QQuickItem
{
    Q_OBJECT
    friend class PlayerRenderer;

public:
    explicit PlayerQuickItem(QQuickItem* parent = nullptr);
    ~PlayerQuickItem() override;
    void initMpv(PlayerComponent* player);
    QString debugInfo() { return m_debugInfo; }

signals:
    void onFatalError(QString message);

private slots:
    void onWindowChanged(QQuickWindow* win);
    void onSynchronize();
    void onInvalidate();
    void onHandleFatalError(QString message);

private:
    mpv::qt::Handle m_mpv;
    mpv_render_context* m_mpvGL;
    PlayerRenderer* m_renderer;
    QString m_debugInfo;
};

#endif
