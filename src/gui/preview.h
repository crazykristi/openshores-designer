//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - 3D preview of a blueprint.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_PREVIEW_H
#define SHIPGEN_PREVIEW_H

#include <QColor>
#include <QHash>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QVector>
#include <QPoint>

#include "auscface.h"

class AuDesign;

// A simple shaded 3D view of a design. The parts are drawn with the engine's
// own triangle data, so what you see is what the game would show.
class DesignPreview : public QOpenGLWidget
{
	Q_OBJECT

public:
	explicit DesignPreview(QWidget* parent = 0);
	~DesignPreview();

	// The design to show. The preview does not take ownership of it.
	// With 'frame' the view is framed on it; without, the camera stays where it is.
	void	SetDesign(const AuDesign* design, bool frame = true);

	// Shows the interior rooms of a spacecraft in a different colour.
	void	SetShowRooms(bool show);

	// How opaque the hull and the site are drawn, from 0 to 1. The default
	// lets the rooms be seen through it.
	void	SetHullOpacity(float opacity);

	// Shows plain triangles instead of a design. This is what the mesh tool
	// uses to show a model that has been thinned out.
	void	SetMesh(const AuScTriList& tris, const QColor& colour);

	// Shows a second mesh over the top of the first one, see through. It is
	// used to show the model that was there before it was thinned out.
	void	SetOverlay(const AuScTriList& tris, const QColor& colour);

	// Frames the view on the whole model.
	void	FrameAll();

signals:
	void	ViewChanged();

protected:
	void	initializeGL() override;
	void	resizeGL(int w, int h) override;
	void	paintGL() override;
	void	mousePressEvent(QMouseEvent* event) override;
	void	mouseMoveEvent(QMouseEvent* event) override;
	void	wheelEvent(QWheelEvent* event) override;

private:
	struct Vertex
	{
		float	x, y, z;		// Position.
		float	nx, ny, nz;	// Normal.
		float	r, g, b;		// Colour.
		float	a;			// Opacity.
		float	u, v;		// Texture coordinate.
		float	tex;		// Texture slot, for the shader.
	};

	// One run of vertices that share a texture. The parts are drawn in
	// texture order so that the texture can be bound once per run.
	struct DrawRange
	{
		int		start = 0;
		int		count = 0;
		int		texture = 0;
		bool	blend = false;
		// The see-through hull: only its nearest layer is drawn (its depth
		// first, then its colour where it is nearest), or its far side
		// showed through its near side wherever it happened to be drawn
		// later.
		bool	nearestOnly = false;
	};

	void	RebuildGeometry();
	void	UploadGeometry();
	void	BuildCameraMatrix(QMatrix4x4& projection, QMatrix4x4& view) const;

	// Loads the design's texture palette into OpenGL. Returns the texture to
	// bind for a slot; a white tile when the slot is empty.
	int		TextureForSlot(int slot);
	void	ReleaseTextures();

	const AuDesign*	m_design = 0;
	bool				m_showRooms = true;
	float				m_hullOpacity = 0.20f;

	// The plain triangles, when there is no design.
	AuScTriList		m_mesh;
	AuScTriList		m_overlay;
	QColor			m_meshColour = QColor(150, 160, 175);
	QColor			m_overlayColour = QColor(220, 120, 90);
	bool				m_hasMesh = false;
	bool				m_hasOverlay = false;

	float				m_glVersion = 0.0;
	QOpenGLShaderProgram*	m_program = 0;
	QOpenGLBuffer			m_vbo;
	QOpenGLVertexArrayObject	m_vao;
	QVector<Vertex>			m_vertices;
	QVector<DrawRange>		m_ranges;
	QVector<QImage>			m_paletteImages;
	QList<QOpenGLTexture*>	m_textures;		// One per palette slot.
	QHash<int, QOpenGLTexture*>	m_texturesBySlot;
	bool					m_whiteTexture = false;
	int						m_solidCount = 0;		// The solid part of the vertex list.
	bool					m_needsUpload = true;

	// Camera.
	float	m_yaw = 30.f;
	float	m_pitch = 20.f;
	float	m_distance = 200.f;
	QPoint	m_center;
	float	m_centerZ = 0.f;
	bool	m_dragging = false;
	QPoint	m_lastMouse;
};

#endif // SHIPGEN_PREVIEW_H
