//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - 3D preview of a blueprint.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_PREVIEW_H
#define SHIPGEN_PREVIEW_H

#include <QColor>
#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QVector3D>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QVector>
#include <QPoint>
#include <QString>

#include "auscface.h"
#include "effects.h"

class AuDesign;

// On Linux, Qt's default OpenGL context is whatever the driver gives
// without asking, and some drivers give one older than the 3.3 the preview's
// shaders need while they would give 3.3 when asked. Called once the
// application exists: keeps the default when it is new enough, otherwise asks
// for a 3.3 core context when the driver has one. Returns what is wrong when
// neither gives OpenGL 3.3 (empty when all is well).
QString ChooseOpenGLFormat();

// Whether a GL_VERSION string says at least major.minor. Drivers write it
// differently: Mesa gives "4.5 (Core Profile) Mesa 25.0.7", NVIDIA gives
// "4.6.0 NVIDIA 580.196.07", so only the first two numbers are read.
bool OpenGLVersionAtLeast(const char* versionText, int major, int minor);

// Shown on start when ChooseOpenGLFormat found a problem: says what is
// missing, links to the issue tracker, and lets the user go on at their own
// risk (or quit: false). "Do not warn again" is remembered per problem.
bool WarnAboutPrerequisites(const QString& problem);

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

	// Turns the camera: degrees round and up, and how much closer than the
	// framed view (2 is twice as close). For screenshots from the command line.
	void	SetViewAngles(float yaw, float pitch, float zoom = 1.f);

	// While on, a click (without dragging) on the hull sends HullPicked
	// instead of only turning the view. The cursor becomes a cross.
	void	SetPicking(bool on);
	bool	Picking() const { return m_picking; }
	// Free flight through the model: W A S D (or the arrow keys) move, Q and
	// E go down and up, Shift goes faster, a drag looks round, the wheel sets
	// the speed and Esc leaves. It starts from where the orbiting view
	// stands, and leaving orbits round the point straight ahead.
	void	SetFreeFly(bool on);
	bool	FreeFly() const { return m_fly; }
	// The paint brush: while on, a left drag over the hull paints (BrushStamp
	// for each point the brush passes, between BrushBegan and BrushEnded),
	// the right button turns the view, and the brush's outline follows the
	// mouse on the hull. 'sizeFeet' is across the brush where it meets the hull.
	void	SetBrush(bool on, double sizeFeet, bool square);
	bool	Brush() const { return m_brush; }

	// The game's animated surfaces (surface effects). Each runs as the game
	// would run it in that situation on this design, and not at all when the
	// game never would; with 'markNever' those faces are tinted, so they can
	// be found. Off, no effect is shown.
	void	SetEffects(bool animate, const EffectSituation& situation, bool markNever);
	// What the design does with its effects (for the designer's list).
	const DesignEffects&	Effects() const { return m_effects; }

signals:
	void	ViewChanged();
	void	FreeFlyChanged(bool on);
	// Where a click met the hull, the hull's face there (turned towards the
	// viewer), and the way back to the viewer. In the blueprint's feet.
	void	HullPicked(const QVector3D& point, const QVector3D& normal, const QVector3D& towardViewer);
	// The brush: where it met the hull, where the eye is and which way is up
	// on the screen, in the blueprint's feet.
	void	BrushBegan();
	void	BrushStamp(const QVector3D& at, const QVector3D& eye, const QVector3D& up);
	void	BrushEnded();

protected:
	void	initializeGL() override;
	void	resizeGL(int w, int h) override;
	void	paintGL() override;
	void	mousePressEvent(QMouseEvent* event) override;
	void	mouseMoveEvent(QMouseEvent* event) override;
	void	mouseReleaseEvent(QMouseEvent* event) override;
	void	wheelEvent(QWheelEvent* event) override;
	void	keyPressEvent(QKeyEvent* event) override;
	void	keyReleaseEvent(QKeyEvent* event) override;
	void	focusOutEvent(QFocusEvent* event) override;
	void	leaveEvent(QEvent* event) override;

private:
	struct Vertex
	{
		float	x, y, z;		// Position.
		float	nx, ny, nz;	// Normal.
		float	r, g, b;		// Colour.
		float	a;			// Opacity.
		float	u, v;		// Texture coordinate.
		float	tex;		// Texture slot, for the shader.
		float	eu, ev;		// Effect texture coordinate.
		float	eint;		// Effect intensity.
		float	eff;		// The surface effect, or -1.
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

	// Why the view cannot draw (empty when it can), shown in the view.
	QString				m_glProblem;
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
	QPoint	m_pressMouse;
	bool	m_picking = false;
	Qt::MouseButton	m_dragButton = Qt::NoButton;

	// The paint brush.
	bool		m_brush = false;
	bool		m_brushSquare = false;
	double		m_brushFeet = 4.0;
	bool		m_brushing = false;		// A stroke under way.
	bool		m_brushHover = false;	// The outline is on the hull at m_brushAt.
	QVector3D	m_brushAt;
	void		Stamp(const QPoint& pixel, bool emitIt);
	void		EyeAndUp(QVector3D& eye, QVector3D& up) const;

	// The hull's triangles for picking, made when first needed.
	mutable AuScTriList	m_pickTris;
	mutable bool		m_pickValid = false;

	// The surface effects: what the design does with them, the situation
	// shown, the frames (one layer per picture of the engine's effect
	// library), and the clock that steps them six times a second.
	DesignEffects		m_effects;
	EffectSituation		m_situation;
	bool				m_animate = true;
	bool				m_markNever = true;
	QOpenGLTexture*		m_effectFrames = 0;
	bool				m_effectFramesTried = false;
	bool				m_effectFramesMissing = false;
	class QTimer*		m_effectTimer = 0;
	int					m_effectSeqFrame = 0;
	int					m_effectRandFrame = 0;
	class QLabel*		m_effectNote = 0;	// What never runs, over the view's corner.
	void	UpdateEffectTimer();
	void	UpdateEffectNote();
	void	PlaceEffectNote();
	void	LoadEffectFrames();
	bool	AnyEffectRuns() const;

	// Free flight.
	bool			m_fly = false;
	QVector3D		m_eye;
	float			m_flySpeed = 20.f;		// Feet a second.
	QSet<int>		m_keys;				// Held down.
	class QTimer*	m_flyTimer = 0;
	QElapsedTimer	m_flyClock;
	QVector3D	OrbitOffset() const;		// From the centre to the eye, of length 1.
	void		FlyStep();

	// The hull point under a pixel, if any.
	bool	PickHull(const QPoint& pixel, QVector3D& point, QVector3D& normal, QVector3D& toward) const;
};

#endif // SHIPGEN_PREVIEW_H
