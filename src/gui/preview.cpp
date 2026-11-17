//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - 3D preview of a blueprint.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "preview.h"

#include <cmath>
#include <functional>

#include <QOpenGLContext>
#include <QCoreApplication>
#include <QPushButton>
#include <QCheckBox>
#include <QMessageBox>
#include <QSettings>
#include <QStringList>
#include <cstdio>
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QPainter>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QMap>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QVector3D>
#include <QVector4D>
#include <QPolygonF>
#include <QPen>
#include <QMatrix4x4>

#include "auglsl.h"
#include "version.h"
#include "auscdesign.h"
#include "auscface.h"
#include "auscparts.h"
#include "audesign.h"

namespace {

const char* kVertexShader =
	"#version 330\n"
	"layout(location=0) in vec3 inPosition;\n"
	"layout(location=1) in vec3 inNormal;\n"
	"layout(location=2) in vec3 inColour;\n"
	"layout(location=3) in float inAlpha;\n"
	"layout(location=4) in vec2 inUv;\n"
	"layout(location=5) in float inTexture;\n"
	"uniform mat4 uMvp;\n"
	"out vec3 vNormal;\n"
	"out vec3 vColour;\n"
	"out float vAlpha;\n"
	"out vec2 vUv;\n"
	"out float vTexture;\n"
	"void main()\n"
	"{\n"
	"    vNormal = inNormal;\n"
	"    vColour = inColour;\n"
	"    vAlpha = inAlpha;\n"
	"    vUv = inUv;\n"
	"    vTexture = inTexture;\n"
	"    gl_Position = uMvp * vec4(inPosition, 1.0);\n"
	"}\n";

const char* kFragmentShader =
	"#version 330\n"
	"in vec3 vNormal;\n"
	"in vec3 vColour;\n"
	"in float vAlpha;\n"
	"in vec2 vUv;\n"
	"in float vTexture;\n"
	"uniform vec3 uLight;\n"
	"uniform sampler2D uTexture;\n"
	"out vec4 fragColour;\n"
	"void main()\n"
	"{\n"
	"    vec3 n = normalize(vNormal);\n"
	"    float d = abs(dot(n, normalize(uLight)));\n"
	"    float a = 0.30 + 0.70 * d;\n"
	"    vec4 tex = vTexture >= 0.0 ? texture(uTexture, vUv) : vec4(1.0);\n"
	"    fragColour = vec4(vColour * tex.rgb * a, vAlpha * tex.a);\n"
	"}\n";

// The colour a part is drawn in.
QColor ColourForPart(const AuScPart* part)
{
	if(part->IsHullMesh())
		return QColor(150, 160, 175);
	if(part->IsRoomMesh())
		return QColor(110, 200, 130);
	if(part->IsRoomVoid())
		return QColor(60, 110, 80);
	if(part->IsHullVoid())
		return QColor(90, 90, 110);
	if(part->IsSite())
		return QColor(110, 140, 90);
	if(part->IsBuildZone())
		return QColor(150, 120, 80);
	if(part->IsDoor())
		return QColor(220, 160, 60);
	if(part->IsWindow())
		return QColor(150, 210, 240);
	if(part->IsBerth())
		return QColor(100, 150, 230);
	if(part->IsConsole() || part->IsCommandStation() || part->IsControlStation())
		return QColor(230, 130, 70);
	if(part->IsCitizenPost() || part->IsGuardPost() || part->IsLivestockPost())
		return QColor(230, 220, 120);
	if(part->IsPath())
		return QColor(200, 200, 200);
	return QColor(170, 170, 170);
}

} // namespace

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|


bool OpenGLVersionAtLeast(const char* versionText, int major, int minor)
{
	if(!versionText)
		return false;
	const QStringList numbers = QString(versionText).section(' ', 0, 0).split('.');
	bool majorOk = false, minorOk = false;
	const int givenMajor = numbers.value(0).toInt(&majorOk);
	const int givenMinor = numbers.value(1).toInt(&minorOk);
	if(!majorOk || !minorOk)
		return false;
	return givenMajor > major || (givenMajor == major && givenMinor >= minor);
}

QString ChooseOpenGLFormat()
{
	struct Probe { bool enough = false; QString driver; };
	auto probe = [](const QSurfaceFormat& format) -> Probe {
		Probe result;
		QOpenGLContext context;
		context.setFormat(format);
		if(!context.create())
			return result;
		QOffscreenSurface surface;
		surface.setFormat(context.format());
		surface.create();
		if(!surface.isValid() || !context.makeCurrent(&surface))
			return result;
		const char* text = reinterpret_cast<const char*>(context.functions()->glGetString(GL_VERSION));
		const char* renderer = reinterpret_cast<const char*>(context.functions()->glGetString(GL_RENDERER));
		result.enough = OpenGLVersionAtLeast(text, 3, 3);
		result.driver = QString("OpenGL %1 (%2)").arg(text ? text : "?", renderer ? renderer : "?");
		context.doneCurrent();
		return result;
	};
	if(QGuiApplication::platformName() == "offscreen")
		return QString();
	const Probe first = probe(QSurfaceFormat::defaultFormat());
	if(first.enough)
		return QString();
#if defined(Q_OS_LINUX)
	QSurfaceFormat core = QSurfaceFormat::defaultFormat();
	core.setRenderableType(QSurfaceFormat::OpenGL);
	core.setVersion(3, 3);
	core.setProfile(QSurfaceFormat::CoreProfile);
	core.setDepthBufferSize(24);
	const Probe second = probe(core);
	if(second.enough)
	{
		QSurfaceFormat::setDefaultFormat(core);
		return QString();
	}
#endif
	if(first.driver.isEmpty())
		return QCoreApplication::translate("DesignPreview", "No OpenGL context could be made on this system, so the 3D view will stay empty. "
				"The graphics driver (OpenGL) may be missing or not working.");
	return QCoreApplication::translate("DesignPreview", "The 3D view needs OpenGL 3.3, but this system's graphics driver gives %1, "
			"so the 3D view will stay empty.").arg(first.driver);
}

bool WarnAboutPrerequisites(const QString& problem)
{
	QSettings settings;
	const QString key = "startup/ignoredProblem";
	if(settings.value(key).toString() == problem)
		return true;
	QMessageBox box;
	box.setIcon(QMessageBox::Warning);
	box.setWindowTitle(QCoreApplication::translate("DesignPreview", "Something this program needs is missing"));
	box.setTextFormat(Qt::RichText);
	box.setText(QCoreApplication::translate("DesignPreview",
			"<p>%1</p><p>Everything else should work, but this system has not been tested. "
			"You can go on at your own risk.</p>"
			"<p>If something does not work, please report it, with what your system is (distribution, graphics card) "
			"and this message, at<br><a href=\"%2\">%2</a></p>").arg(problem.toHtmlEscaped(), SHIPGEN_ISSUES_URL));
	box.setTextInteractionFlags(Qt::TextBrowserInteraction);
	QCheckBox* again = new QCheckBox(QCoreApplication::translate("DesignPreview", "Do not warn me about this again"));
	box.setCheckBox(again);
	QPushButton* go = box.addButton(QCoreApplication::translate("DesignPreview", "Continue at my own risk"), QMessageBox::AcceptRole);
	box.addButton(QCoreApplication::translate("DesignPreview", "Quit"), QMessageBox::RejectRole);
	box.setDefaultButton(go);
	box.exec();
	if(box.clickedButton() != go)
		return false;
	if(again->isChecked())
		settings.setValue(key, problem);
	return true;
}

DesignPreview::DesignPreview(QWidget* parent) :
	QOpenGLWidget(parent)
{
	setMinimumSize(320, 240);
	setFocusPolicy(Qt::StrongFocus);
}

DesignPreview::~DesignPreview()
{
	makeCurrent();
	ReleaseTextures();
	doneCurrent();
	if(m_program)
		delete m_program;
}

void DesignPreview::ReleaseTextures()
{
	for(QOpenGLTexture* texture : m_textures)
		delete texture;
	m_textures.clear();
	m_texturesBySlot.clear();
	m_whiteTexture = false;
}

int DesignPreview::TextureForSlot(int slot)
{
	// A plain white tile stands in for a slot with no image, so that the
	// shader can always sample and the part keeps its own colour.
	if(slot < 0 || slot >= m_paletteImages.size() || m_paletteImages.at(slot).isNull())
	{
		if(m_whiteTexture)
			return -1;
		QImage white(1, 1, QImage::Format_ARGB32);
		white.fill(Qt::white);
		QOpenGLTexture* texture = new QOpenGLTexture(white);
		texture->setMinificationFilter(QOpenGLTexture::Linear);
		texture->setMagnificationFilter(QOpenGLTexture::Linear);
		m_textures.append(texture);
		m_texturesBySlot.insert(-1, texture);
		m_whiteTexture = true;
		return -1;
	}

	if(m_texturesBySlot.contains(slot))
		return slot;

	// The game uploads images bottom row first (texture v = 0 is the bottom
	// of the image); QOpenGLTexture starts at the top, so flip to match.
	QOpenGLTexture* texture = new QOpenGLTexture(m_paletteImages.at(slot).mirrored());
	texture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
	texture->setMagnificationFilter(QOpenGLTexture::Linear);
	texture->setWrapMode(QOpenGLTexture::Repeat);
	texture->generateMipMaps();
	m_textures.append(texture);
	m_texturesBySlot.insert(slot, texture);
	return slot;
}

void DesignPreview::SetDesign(const AuDesign* design, bool frame)
{
	m_design = design;
	m_pickValid = false;
	// The palette may be different, so any textures that were loaded for the
	// old design have to go.
	if(context())
	{
		makeCurrent();
		ReleaseTextures();
		doneCurrent();
	}
	RebuildGeometry();
	if(frame)
		FrameAll();
	update();
}

void DesignPreview::SetShowRooms(bool show)
{
	m_showRooms = show;
	RebuildGeometry();
	update();
}

void DesignPreview::SetHullOpacity(float opacity)
{
	m_hullOpacity = opacity;
	RebuildGeometry();
	update();
}

void DesignPreview::SetMesh(const AuScTriList& tris, const QColor& colour)
{
	m_mesh = tris;
	m_meshColour = colour;
	m_hasMesh = true;
	RebuildGeometry();
	FrameAll();
	update();
}

void DesignPreview::SetOverlay(const AuScTriList& tris, const QColor& colour)
{
	m_overlay = tris;
	m_overlayColour = colour;
	m_hasOverlay = true;
	RebuildGeometry();
	update();
}

void DesignPreview::initializeGL()
{
	// The preview needs OpenGL 3.3 for the shaders. Older drivers still get
	// the rest of the tool, just without the 3D view.
	QOpenGLFunctions* functions = context() ? context()->functions() : 0;
	if(!functions)
	{
		m_glProblem = tr("No OpenGL context could be made.");
		fprintf(stderr, "3D view: %s\n", qPrintable(m_glProblem));
		return;
	}
	const char* versionText = reinterpret_cast<const char*>(functions->glGetString(GL_VERSION));
	const char* rendererText = reinterpret_cast<const char*>(functions->glGetString(GL_RENDERER));
	const QString driver = QString("OpenGL %1, %2").arg(versionText ? versionText : "?", rendererText ? rendererText : "?");
	if(!OpenGLVersionAtLeast(versionText, 3, 3))
	{
		m_glProblem = tr("The 3D view needs OpenGL 3.3; this driver gives %1.").arg(driver);
		fprintf(stderr, "3D view: %s\n", qPrintable(m_glProblem));
		return;
	}
	m_program = new QOpenGLShaderProgram(this);
	m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader);
	m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader);
	if(!m_program->link())
	{
		m_glProblem = tr("The 3D view's shaders did not compile on %1: %2").arg(driver, m_program->log().trimmed());
		fprintf(stderr, "3D view: %s\n", qPrintable(m_glProblem));
		delete m_program;
		m_program = 0;
		return;
	}
	// A core-profile context needs a vertex array object, and it is harmless
	// on the compatibility profiles that Linux drivers usually give us.
	m_vao.create();
	m_vbo.create();
	UploadGeometry();

	functions->glEnable(GL_DEPTH_TEST);
	// The faces of a blueprint are not always wound the same way, so both
	// sides are drawn.
	functions->glDisable(GL_CULL_FACE);
}

void DesignPreview::resizeGL(int w, int h)
{
	Q_UNUSED(w);
	Q_UNUSED(h);
}

// A ball at 'at' and an arrow from it along 'dir'.
static void AddContrailMarker(const AuPoint& at, const AuPoint& dir, double radius, const QColor&,
		const std::function<void(const AuScPoint&)>& put)
{
	AuPoint w = dir;
	w.Unit();
	AuPoint u = fabs(w.z) < .9 ? w.Cross(AuPoint(0, 0, 1)) : w.Cross(AuPoint(1, 0, 0));
	u.Unit();
	const AuPoint v = w.Cross(u);
	auto tri = [&put](const AuPoint& a, const AuPoint& b, const AuPoint& c, const AuPoint& na, const AuPoint& nb, const AuPoint& nc) {
		put(AuScPoint(a, 0.f, 0.f, na));
		put(AuScPoint(b, 0.f, 0.f, nb));
		put(AuScPoint(c, 0.f, 0.f, nc));
	};
	const int around = 12, down = 8;
	auto onBall = [&](int i, int j) {
		const double a = 2 * M_PI * i / around, b = M_PI * j / down;
		return (u * cos(a) + v * sin(a)) * sin(b) + w * cos(b);
	};
	for(int j = 0; j < down; ++j)
		for(int i = 0; i < around; ++i)
		{
			const AuPoint n0 = onBall(i, j), n1 = onBall(i + 1, j), n2 = onBall(i + 1, j + 1), n3 = onBall(i, j + 1);
			tri(at + n0 * radius, at + n3 * radius, at + n2 * radius, n0, n3, n2);
			tri(at + n0 * radius, at + n2 * radius, at + n1 * radius, n0, n2, n1);
		}
	const double shaft = radius * .3, head = radius * .8, shaftEnd = radius * 3.2, tip = radius * 4.6;
	auto ring = [&](int i) {
		const double a = 2 * M_PI * i / around;
		return u * cos(a) + v * sin(a);
	};
	for(int i = 0; i < around; ++i)
	{
		const AuPoint n0 = ring(i), n1 = ring(i + 1);
		const AuPoint a0 = at + n0 * shaft, a1 = at + n1 * shaft;
		const AuPoint b0 = a0 + w * shaftEnd, b1 = a1 + w * shaftEnd;
		tri(a0, a1, b1, n0, n1, n1);
		tri(a0, b1, b0, n0, n1, n0);
		const AuPoint c0 = at + w * shaftEnd + n0 * head, c1 = at + w * shaftEnd + n1 * head, top = at + w * tip;
		tri(c0, c1, top, n0, n1, w);
		tri(at + w * shaftEnd, c1, c0, w * -1.0, w * -1.0, w * -1.0);
	}
}

void DesignPreview::RebuildGeometry()
{
	// Everything is built again from scratch, so the old lists go first.
	m_vertices.clear();
	m_ranges.clear();

	// Vertices are grouped by texture, because the shader is given one
	// texture at a time. That keeps a model with a dozen textures to a
	// dozen draw calls without needing a texture array.
	QMap<int, QVector<Vertex> > opaque;
	QMap<int, QVector<Vertex> > blend;
	QMap<int, QVector<Vertex> > seeThroughHull;		// Blended, nearest layer only.

	auto addVertex = [](QVector<Vertex>& list, const AuScPoint& p, const QColor& colour,
			float alpha, int slot)
	{
		Vertex vert;
		vert.x = float(p.x);
		vert.y = float(p.y);
		vert.z = float(p.z);
		vert.nx = float(p.m_N.x);
		vert.ny = float(p.m_N.y);
		vert.nz = float(p.m_N.z);
		vert.r = float(colour.redF());
		vert.g = float(colour.greenF());
		vert.b = float(colour.blueF());
		vert.a = alpha;
		vert.u = p.m_Tx;
		vert.v = p.m_Ty;
		vert.tex = slot < 0 ? -1.f : float(slot);
		list << vert;
	};

	if(m_hasMesh)
	{
		// A plain model has no palette, so the shader gets white and the
		// vertex colour does the work.
		for(const AuScTri& t : m_mesh)
		{
			const AuScPoint* v[3] = { &t.V0(), &t.V1(), &t.V2() };
			for(int i = 0; i < 3; ++i)
				addVertex(opaque[-1], *v[i], m_meshColour, 1.f, -1);
		}
		if(m_hasOverlay)
		{
			for(const AuScTri& t : m_overlay)
			{
				const AuScPoint* v[3] = { &t.V0(), &t.V1(), &t.V2() };
				for(int i = 0; i < 3; ++i)
					addVertex(blend[-1], *v[i], m_overlayColour, 0.25f, -1);
			}
		}
	}
	else if(m_design)
	{
		// The palette of the design. A slot without an image is drawn white,
		// which is what makes an unpainted hull keep its own colour.
		m_paletteImages.resize(SCTEXI_Count);
		for(int i = 0; i < SCTEXI_Count; ++i)
			m_paletteImages[i] = m_design->TexturePaletteImage(i);

		QColor themeColors[AuScFace::THEME_Count];
		for(int i = 0; i < AuScFace::THEME_Count; ++i)
			themeColors[i] = QColor(170, 170, 170);

		const AuScParts& parts = m_design->Parts();

		// Which palette slot a face should be drawn with. A face that has no
		// texture coordinates would sample the corner of whatever image sits
		// in slot 0, so unpainted parts are drawn without a texture.
		auto textureSlot = [this](const AuScTriRender& t) -> int
		{
			int slot = int(t.TextureIndex());
			if(slot < 0 || slot >= m_paletteImages.size() || m_paletteImages.at(slot).isNull())
				return -1;
			const bool uv0 = t.V0().m_Tx != 0.f || t.V0().m_Ty != 0.f;
			const bool uv1 = t.V1().m_Tx != 0.f || t.V1().m_Ty != 0.f;
			const bool uv2 = t.V2().m_Tx != 0.f || t.V2().m_Ty != 0.f;
			if(!uv0 && !uv1 && !uv2)
				return -1;
			return slot;
		};

		QList<const AuScPart*> contrails;
		for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
		{
			const AuScPart* part = it.value();
			if(!part)
				continue;
			if(part->IsRoomVoid() || part->IsHullVoid() || part->IsEnvironmentVoid())
				continue;
			if(part->IsContrailEmitter() && !part->IsOmitted())
			{
				contrails << part;
				continue;
			}
			if(part->IsRoomMesh() && !m_showRooms)
				continue;
			if(!part->IsVisibleInFinalModel() && !part->IsHullMesh() && !part->IsRoomMesh() &&
				!part->IsSite() && !part->IsBuildZone() && !part->IsDoor() && !part->IsWindow())
				continue;

			AuScTriRenderList opaqueTris, transparentTris;
			part->RenderTriangles(opaqueTris, transparentTris, themeColors);

			// The hull and the site are drawn see through so that the inside
			// of the ship or building can be seen.
			const bool seeThrough = part->IsHullMesh() || part->IsSite();
			// A decal shows its own image, as solid as the game draws it.
			const bool decal = part->IsDecal();
			const QColor colour = decal ? QColor(255, 255, 255) : ColourForPart(part);
			for(const AuScTriRender& t : opaqueTris.values())
			{
				const int slot = textureSlot(t);
				const AuScPoint* v[3] = { &t.V0(), &t.V1(), &t.V2() };
				// A painted face is drawn in its paint.
				const QColor faceColour = t.Paint().alpha() > 0 ? t.Paint() : colour;
				for(int i = 0; i < 3; ++i)
					addVertex(seeThrough ? seeThroughHull[slot] : opaque[slot], *v[i], faceColour,
							seeThrough ? m_hullOpacity : 1.f, slot);
			}
			// Transparent parts of the model, such as windows, have their own
			// sorts of blending but are drawn the same way here.
			for(const AuScTriRender& t : transparentTris.values())
			{
				const int slot = textureSlot(t);
				const AuScPoint* v[3] = { &t.V0(), &t.V1(), &t.V2() };
				for(int i = 0; i < 3; ++i)
					addVertex(blend[slot], *v[i], colour, decal ? t.Opacity() : 0.5f, slot);
			}
		}

		// The contrail emitters are flat icons the game does not show: each
		// is drawn as a ball with an arrow the way the trail goes, big enough
		// to find on a big ship.
		if(!contrails.isEmpty())
		{
			AuPoint mn(1e30, 1e30, 1e30), mx(-1e30, -1e30, -1e30);
			for(const QMap<int, QVector<Vertex> >* lists : { &opaque, &seeThroughHull })
				for(const QVector<Vertex>& list : *lists)
					for(const Vertex& v : list)
					{
						mn = mn.Min(AuPoint(v.x, v.y, v.z));
						mx = mx.Max(AuPoint(v.x, v.y, v.z));
					}
			const AuPoint middle = (mn + mx) / 2.0;
			const double longest = qMax(1.0, qMax(mx.x - mn.x, qMax(mx.y - mn.y, mx.z - mn.z)));
			for(const AuScPart* part : contrails)
			{
				AuScFaceList faces;
				part->Faces(faces);
				AuScTriList tris;
				faces.Triangles(tris);
				if(tris.isEmpty())
					continue;
				const AuPoint at = part->SoundEmitLoc();
				AuPoint dir = tris.first().Normal();
				if(dir.Dot(at - middle) < 0)
					dir = dir * -1.0;
				const double radius = qMax(double(part->EmitterDiameter()) * .5, longest * .012);
				AddContrailMarker(at, dir, radius, QColor(90, 190, 255), [&](const AuScPoint& p) {
					addVertex(opaque[-1], p, QColor(90, 190, 255), 1.f, -1);
				});
			}
		}
	}

	// The solid parts come first so that they can be drawn first. Only
	// non-empty runs become draw calls.
	for(QMap<int, QVector<Vertex> >::const_iterator it = opaque.constBegin(); it != opaque.constEnd(); ++it)
	{
		if(it.value().isEmpty())
			continue;
		DrawRange range;
		range.start = m_vertices.size();
		range.count = it.value().size();
		range.texture = it.key();
		range.blend = false;
		m_vertices += it.value();
		m_ranges << range;
	}
	m_solidCount = m_vertices.size();
	for(int pass = 0; pass < 2; ++pass)
	{
		const QMap<int, QVector<Vertex> >& lists = pass == 0 ? seeThroughHull : blend;
		for(QMap<int, QVector<Vertex> >::const_iterator it = lists.constBegin(); it != lists.constEnd(); ++it)
		{
			if(it.value().isEmpty())
				continue;
			DrawRange range;
			range.start = m_vertices.size();
			range.count = it.value().size();
			range.texture = it.key();
			range.blend = true;
			range.nearestOnly = pass == 0;
			m_vertices += it.value();
			m_ranges << range;
		}
	}

	m_needsUpload = true;
	// The buffer can only be filled when the widget's context is current.
	// paintGL does it when it is not, so nothing is lost.
	if(m_program && context() && QOpenGLContext::currentContext() == context())
		UploadGeometry();
}

void DesignPreview::UploadGeometry()
{
	if(!m_vbo.isCreated())
		m_vbo.create();
	m_vbo.bind();
	if(m_vertices.isEmpty())
		m_vbo.allocate(0);
	else
		m_vbo.allocate(m_vertices.constData(), m_vertices.size() * int(sizeof(Vertex)));
	m_vbo.release();
	m_needsUpload = false;
}

void DesignPreview::FrameAll()
{
	// The plain triangles are framed on their own.
	if(m_hasMesh)
	{
		AuPoint min, max;
		bool have = false;
		for(const AuScTri& t : m_mesh)
		{
			const AuScPoint* v[3] = { &t.V0(), &t.V1(), &t.V2() };
			for(int i = 0; i < 3; ++i)
			{
				if(!have)
				{
					min = AuPoint(v[i]->x, v[i]->y, v[i]->z);
					max = min;
					have = true;
					continue;
				}
				min.x = qMin(min.x, v[i]->x);
				min.y = qMin(min.y, v[i]->y);
				min.z = qMin(min.z, v[i]->z);
				max.x = qMax(max.x, v[i]->x);
				max.y = qMax(max.y, v[i]->y);
				max.z = qMax(max.z, v[i]->z);
			}
		}
		if(!have)
			return;
		m_center = QPoint(int((min.x + max.x) / 2), int((min.y + max.y) / 2));
		m_centerZ = float((min.z + max.z) / 2);
		const double size = max.Distance(min);
		// The model has to fit in the view, which is at an angle, so it
		// needs a little more room than its own size.
		m_distance = float(qMax(0.5, size * 1.6));
		emit ViewChanged();
		return;
	}

	if(!m_design)
		return;
	AuPoint min, max;
	bool have = false;
	const AuScParts& parts = m_design->Parts();
	for(AuScParts::const_iterator it = parts.constBegin(); it != parts.constEnd(); ++it)
	{
		const AuScPart* part = it.value();
		if(!part || !part->IsHullMesh() && !part->IsRoomMesh() && !part->IsSite())
			continue;
		AuPoint pmin, pmax;
		pmin = AuPoint(0,0,0);
		pmax = AuPoint(0,0,0);
		part->ModelExtents(pmin, pmax);
		if(!have)
		{
			min = pmin;
			max = pmax;
			have = true;
		}
		else
		{
			min = min.Min(pmin);
			max = max.Max(pmax);
		}
	}
	if(!have)
		return;
	m_center = QPoint(int((min.x + max.x) / 2), int((min.y + max.y) / 2));
	m_centerZ = float((min.z + max.z) / 2);
	const double size = max.Distance(min);
	// The field of view is 45 degrees up and down; a view that is taller
	// than it is wide sees less from side to side, so it stands further back.
	const double narrow = qMax(1.0, height() / qMax(1.0, double(width())));
	m_distance = float(qMax(20.0, size * 1.1 * narrow));
	emit ViewChanged();
}

void DesignPreview::BuildCameraMatrix(QMatrix4x4& projection, QMatrix4x4& view) const
{
	const double fov = 45.0;
	// Inside the ship the near plane has to be close, or walls vanish.
	const double nearP = m_fly ? 0.2 : 1.0;
	const double farP = m_distance * 10.0 + 1000.0;
	projection.perspective(fov, width() / qMax(1.0, double(height())), nearP, farP);

	if(m_fly)
	{
		view.lookAt(m_eye, m_eye - OrbitOffset(), QVector3D(0, 0, 1));
		return;
	}
	QVector3D eye = OrbitOffset() * m_distance;
	eye += QVector3D(m_center.x(), m_center.y(), m_centerZ);
	const QVector3D target(m_center.x(), m_center.y(), m_centerZ);
	view.lookAt(eye, target, QVector3D(0, 0, 1));
}

QVector3D DesignPreview::OrbitOffset() const
{
	return QVector3D(qSin(qDegreesToRadians(m_yaw)) * qCos(qDegreesToRadians(m_pitch)),
			qCos(qDegreesToRadians(m_yaw)) * qCos(qDegreesToRadians(m_pitch)),
			qSin(qDegreesToRadians(m_pitch)));
}

void DesignPreview::SetFreeFly(bool on)
{
	if(on == m_fly)
		return;
	const QVector3D centre(m_center.x(), m_center.y(), m_centerZ);
	if(on)
	{
		m_eye = centre + OrbitOffset() * m_distance;
		m_flySpeed = qBound(5.f, m_distance / 10.f, 200.f);
		if(!m_flyTimer)
		{
			m_flyTimer = new QTimer(this);
			m_flyTimer->setInterval(16);
			connect(m_flyTimer, &QTimer::timeout, this, &DesignPreview::FlyStep);
		}
		m_keys.clear();
		m_flyClock.start();
		m_flyTimer->start();
		setFocus();
	}
	else
	{
		// Orbit round the point straight ahead, as far ahead as the
		// orbit stood back.
		const QVector3D ahead = m_eye - OrbitOffset() * m_distance;
		m_center = QPoint(qRound(ahead.x()), qRound(ahead.y()));
		m_centerZ = ahead.z();
		if(m_flyTimer)
			m_flyTimer->stop();
		m_keys.clear();
	}
	m_fly = on;
	update();
	emit FreeFlyChanged(on);
	emit ViewChanged();
}

void DesignPreview::FlyStep()
{
	const float dt = qMin(0.1f, m_flyClock.restart() / 1000.f);
	if(m_keys.isEmpty())
		return;
	const QVector3D forward = -OrbitOffset();
	QVector3D flat(forward.x(), forward.y(), 0);
	flat = flat.lengthSquared() > 1e-6f ? flat.normalized() : QVector3D(0, 1, 0);
	const QVector3D right = QVector3D::crossProduct(flat, QVector3D(0, 0, 1));
	QVector3D move;
	if(m_keys.contains(Qt::Key_W) || m_keys.contains(Qt::Key_Up))
		move += forward;
	if(m_keys.contains(Qt::Key_S) || m_keys.contains(Qt::Key_Down))
		move -= forward;
	if(m_keys.contains(Qt::Key_D) || m_keys.contains(Qt::Key_Right))
		move += right;
	if(m_keys.contains(Qt::Key_A) || m_keys.contains(Qt::Key_Left))
		move -= right;
	if(m_keys.contains(Qt::Key_E) || m_keys.contains(Qt::Key_PageUp))
		move += QVector3D(0, 0, 1);
	if(m_keys.contains(Qt::Key_Q) || m_keys.contains(Qt::Key_PageDown))
		move -= QVector3D(0, 0, 1);
	if(move.lengthSquared() < 1e-6f)
		return;
	const float speed = m_flySpeed * (m_keys.contains(Qt::Key_Shift) ? 4.f : 1.f);
	m_eye += move.normalized() * speed * dt;
	update();
	emit ViewChanged();
}

void DesignPreview::keyPressEvent(QKeyEvent* event)
{
	if(!m_fly)
	{
		QOpenGLWidget::keyPressEvent(event);
		return;
	}
	if(event->key() == Qt::Key_Escape)
	{
		SetFreeFly(false);
		return;
	}
	if(!event->isAutoRepeat())
		m_keys.insert(event->key());
}

void DesignPreview::keyReleaseEvent(QKeyEvent* event)
{
	if(!m_fly)
	{
		QOpenGLWidget::keyReleaseEvent(event);
		return;
	}
	if(!event->isAutoRepeat())
		m_keys.remove(event->key());
}

void DesignPreview::focusOutEvent(QFocusEvent* event)
{
	m_keys.clear();		// Or a key let go elsewhere keeps the flight going.
	QOpenGLWidget::focusOutEvent(event);
}

void DesignPreview::paintGL()
{
	QOpenGLFunctions* functions = context()->functions();
	if(!functions)
		return;
	// QPainter (the brush's outline, the free flight help) leaves the depth
	// test off and blending on: set what the drawing needs every frame.
	functions->glEnable(GL_DEPTH_TEST);
	functions->glDepthFunc(GL_LESS);
	functions->glDepthMask(true);
	functions->glDisable(GL_BLEND);
	functions->glDisable(GL_CULL_FACE);
	functions->glColorMask(true, true, true, true);
	functions->glClearColor(0.12f, 0.13f, 0.16f, 1.f);
	functions->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if(!m_glProblem.isEmpty())
	{
		QPainter painter(this);
		painter.setPen(QColor(220, 220, 220));
		painter.drawText(rect().adjusted(20, 20, -20, -20), Qt::AlignCenter | Qt::TextWordWrap,
				m_glProblem + "\n\n" + tr("Everything else works as usual."));
		return;
	}
	if(!m_program || m_vertices.isEmpty())
		return;
	if(m_needsUpload)
		UploadGeometry();

	QMatrix4x4 projection, view;
	BuildCameraMatrix(projection, view);
	const QMatrix4x4 mvp = projection * view;

	m_program->bind();
	m_program->setUniformValue("uMvp", mvp);
	m_program->setUniformValue("uLight", QVector3D(0.4f, 0.3f, 0.9f));
	m_program->setUniformValue("uTexture", 0);

	m_vao.bind();
	m_vbo.bind();
	const int stride = sizeof(Vertex);
	m_program->setAttributeBuffer(0, GL_FLOAT, 0, 3, stride);
	m_program->setAttributeBuffer(1, GL_FLOAT, int(sizeof(float) * 3), 3, stride);
	m_program->setAttributeBuffer(2, GL_FLOAT, int(sizeof(float) * 6), 3, stride);
	m_program->setAttributeBuffer(3, GL_FLOAT, int(sizeof(float) * 9), 1, stride);
	m_program->setAttributeBuffer(4, GL_FLOAT, int(sizeof(float) * 10), 2, stride);
	m_program->setAttributeBuffer(5, GL_FLOAT, int(sizeof(float) * 12), 1, stride);
	m_program->enableAttributeArray(0);
	m_program->enableAttributeArray(1);
	m_program->enableAttributeArray(2);
	m_program->enableAttributeArray(3);
	m_program->enableAttributeArray(4);
	m_program->enableAttributeArray(5);

	// Each run of vertices shares one texture. The solid parts come first
	// and then the see-through ones, which is the order they are in.
	bool hullDepth = false;
	for(const DrawRange& range : m_ranges)
	{
		// After the solid parts, the see-through hull's depth, before any of
		// it is coloured in: then only its nearest layer passes the depth
		// test (GL_LEQUAL), and what is solid inside it still shows.
		if(range.blend && !hullDepth)
		{
			hullDepth = true;
			functions->glColorMask(false, false, false, false);
			for(const DrawRange& r : m_ranges)
				if(r.nearestOnly)
					functions->glDrawArrays(GL_TRIANGLES, r.start, r.count);
			functions->glColorMask(true, true, true, true);
			functions->glDepthFunc(GL_LEQUAL);
		}
		if(!m_texturesBySlot.contains(range.texture))
			TextureForSlot(range.texture);
		QOpenGLTexture* texture = m_texturesBySlot.value(range.texture, 0);
		if(texture)
			texture->bind(0);

		if(range.blend)
		{
			functions->glEnable(GL_BLEND);
			functions->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			functions->glDepthMask(false);
		}
		functions->glDrawArrays(GL_TRIANGLES, range.start, range.count);
		if(range.blend)
		{
			functions->glDepthMask(true);
			functions->glDisable(GL_BLEND);
		}
		if(texture)
			texture->release();
	}
	functions->glDepthFunc(GL_LESS);
	m_vbo.release();
	m_vao.release();
	m_program->release();

	if(m_brush && m_brushHover && !m_fly)
	{
		// The brush's outline where it meets the hull: square to the view,
		// as the stamps are.
		QMatrix4x4 projection, viewM;
		BuildCameraMatrix(projection, viewM);
		const QMatrix4x4 mvp = projection * viewM;
		QVector3D eye, up;
		EyeAndUp(eye, up);
		const QVector3D d = (m_brushAt - eye).normalized();
		const QVector3D u = (up - d * QVector3D::dotProduct(up, d)).normalized();
		const QVector3D r = QVector3D::crossProduct(d, u);
		const float radius = float(m_brushFeet / 2);
		QPolygonF outline;
		const int corners = m_brushSquare ? 4 : 48;
		for(int i = 0; i <= corners; ++i)
		{
			QVector3D p;
			if(m_brushSquare)
			{
				static const float sa[] = { -1, 1, 1, -1, -1 }, sb[] = { -1, -1, 1, 1, -1 };
				p = m_brushAt + r * (sa[i] * radius) + u * (sb[i] * radius);
			}
			else
			{
				const float a = float(2 * M_PI * i / corners);
				p = m_brushAt + r * (radius * std::cos(a)) + u * (radius * std::sin(a));
			}
			const QVector4D c = mvp * QVector4D(p, 1.f);
			if(c.w() <= 1e-4f)
				continue;
			outline << QPointF((c.x() / c.w() + 1.f) * .5f * width(), (1.f - c.y() / c.w()) * .5f * height());
		}
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setPen(QPen(QColor(0, 0, 0, 160), 3));
		painter.drawPolyline(outline);
		painter.setPen(QPen(QColor(255, 255, 255, 230), 1.5));
		painter.drawPolyline(outline);
	}
	if(m_fly)
	{
		QPainter painter(this);
		painter.setPen(QColor(230, 230, 230));
		painter.drawText(rect().adjusted(10, 8, -10, -8), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
				tr("Free flight: W A S D move, Q E down and up, Shift faster, drag to look round, "
				   "wheel: speed (%1 ft/s), Esc leaves.").arg(m_flySpeed, 0, 'f', 0));
	}
}

void DesignPreview::mousePressEvent(QMouseEvent* event)
{
	if(m_brush && !m_fly && event->button() == Qt::LeftButton)
	{
		m_brushing = true;
		emit BrushBegan();
		Stamp(event->pos(), true);
		return;
	}
	// With the brush the right button turns the view; otherwise the left.
	if(event->button() == (m_brush ? Qt::RightButton : Qt::LeftButton) ||
			(m_fly && event->button() == Qt::LeftButton))
	{
		m_dragging = true;
		m_dragButton = event->button();
		m_lastMouse = event->pos();
		m_pressMouse = event->pos();
	}
}

void DesignPreview::mouseReleaseEvent(QMouseEvent* event)
{
	if(m_brushing && event->button() == Qt::LeftButton)
	{
		m_brushing = false;
		emit BrushEnded();
		return;
	}
	if(m_dragging && event->button() == m_dragButton && event->button() != Qt::LeftButton)
	{
		m_dragging = false;
		return;
	}
	if(event->button() != Qt::LeftButton)
		return;
	m_dragging = false;
	if(!m_picking || (event->pos() - m_pressMouse).manhattanLength() > 4)
		return;
	QVector3D point, normal, toward;
	if(PickHull(event->pos(), point, normal, toward))
		emit HullPicked(point, normal, toward);
}

void DesignPreview::SetPicking(bool on)
{
	m_picking = on;
	setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
}

bool DesignPreview::PickHull(const QPoint& pixel, QVector3D& point, QVector3D& normal, QVector3D& toward) const
{
	if(!m_design || width() <= 0 || height() <= 0)
		return false;
	QMatrix4x4 projection, view;
	BuildCameraMatrix(projection, view);
	bool invertible = false;
	const QMatrix4x4 inverse = (projection * view).inverted(&invertible);
	if(!invertible)
		return false;
	const float nx = 2.f * float(pixel.x()) / float(width()) - 1.f;
	const float ny = 1.f - 2.f * float(pixel.y()) / float(height());
	const QVector3D nearP = inverse.map(QVector3D(nx, ny, -1.f));
	const QVector3D farP = inverse.map(QVector3D(nx, ny, 1.f));
	const QVector3D dir = (farP - nearP).normalized();

	if(!m_pickValid)
	{
		m_pickTris.clear();
		for(AuScParts::const_iterator it = m_design->Parts().constBegin(); it != m_design->Parts().constEnd(); ++it)
		{
			const AuScPart* part = it.value();
			if(!part || !part->IsHullMesh() || part->IsOmitted())
				continue;
			AuScFaceList faces;
			part->Faces(faces);
			AuScTriList tris;
			faces.Triangles(tris);
			for(const AuScTri& t : tris)
				m_pickTris.append(t);
		}
		m_pickValid = true;
	}
	double best = 1e30;
	{
		for(const AuScTri& t : m_pickTris)
		{
			// Moller-Trumbore, both sides.
			const QVector3D v0(t.V0().x, t.V0().y, t.V0().z), v1(t.V1().x, t.V1().y, t.V1().z), v2(t.V2().x, t.V2().y, t.V2().z);
			const QVector3D e1 = v1 - v0, e2 = v2 - v0;
			const QVector3D p = QVector3D::crossProduct(dir, e2);
			const float det = QVector3D::dotProduct(e1, p);
			if(qAbs(det) < 1e-9f)
				continue;
			const float inv = 1.f / det;
			const QVector3D sv = nearP - v0;
			const float u = QVector3D::dotProduct(sv, p) * inv;
			if(u < 0 || u > 1)
				continue;
			const QVector3D q = QVector3D::crossProduct(sv, e1);
			const float v = QVector3D::dotProduct(dir, q) * inv;
			if(v < 0 || u + v > 1)
				continue;
			const float dist = QVector3D::dotProduct(e2, q) * inv;
			if(dist <= 0 || dist >= best)
				continue;
			best = dist;
			point = nearP + dir * dist;
			normal = QVector3D::crossProduct(e1, e2).normalized();
			if(QVector3D::dotProduct(normal, dir) > 0)
				normal = -normal;
		}
	}
	if(best >= 1e30)
		return false;
	toward = -dir;
	return true;
}

void DesignPreview::SetBrush(bool on, double sizeFeet, bool square)
{
	m_brush = on;
	m_brushFeet = qMax(0.1, sizeFeet);
	m_brushSquare = square;
	setMouseTracking(on);
	setCursor(on ? Qt::CrossCursor : (m_picking ? Qt::CrossCursor : Qt::ArrowCursor));
	if(!on)
	{
		m_brushHover = false;
		if(m_brushing)
		{
			m_brushing = false;
			emit BrushEnded();
		}
	}
	update();
}

// The eye, and up on the screen, in the design's feet.
void DesignPreview::EyeAndUp(QVector3D& eye, QVector3D& up) const
{
	QMatrix4x4 projection, view;
	BuildCameraMatrix(projection, view);
	const QMatrix4x4 inverse = view.inverted();
	eye = inverse.map(QVector3D(0, 0, 0));
	up = inverse.mapVector(QVector3D(0, 1, 0)).normalized();
}

// The brush over a pixel: the outline goes there, and a stamp is sent.
void DesignPreview::Stamp(const QPoint& pixel, bool emitIt)
{
	QVector3D point, normal, toward;
	m_brushHover = PickHull(pixel, point, normal, toward);
	if(m_brushHover)
	{
		m_brushAt = point;
		if(emitIt)
		{
			QVector3D eye, up;
			EyeAndUp(eye, up);
			emit BrushStamp(point, eye, up);
		}
	}
	update();
}

void DesignPreview::leaveEvent(QEvent* event)
{
	m_brushHover = false;
	update();
	QOpenGLWidget::leaveEvent(event);
}

void DesignPreview::mouseMoveEvent(QMouseEvent* event)
{
	if(m_brush && !m_fly && !m_dragging)
	{
		Stamp(event->pos(), m_brushing);
		return;
	}
	if(!m_dragging)
		return;
	const QPoint delta = event->pos() - m_lastMouse;
	m_lastMouse = event->pos();
	const float turn = m_fly ? .3f : .5f;
	m_yaw += delta.x() * turn;
	m_pitch = qBound(-89.f, m_pitch + delta.y() * turn, 89.f);
	update();
	emit ViewChanged();
}

void DesignPreview::wheelEvent(QWheelEvent* event)
{
	const int steps = event->angleDelta().y() / 120;
	if(m_fly)
	{
		m_flySpeed = qBound(1.f, float(m_flySpeed * qPow(1.25, steps)), 1000.f);
		update();
		return;
	}
	m_distance *= qPow(0.8, steps);
	m_distance = qBound(1.f, m_distance, 100000.f);
	update();
	emit ViewChanged();
}

void DesignPreview::SetViewAngles(float yaw, float pitch, float zoom)
{
	m_yaw = yaw;
	m_pitch = qBound(-89.f, pitch, 89.f);
	if(zoom > 0)
		m_distance /= zoom;
	update();
	emit ViewChanged();
}
