/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with self program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <assert.h>

#include "EditorView.h"
#include "ui_local.h"
#include "client.h"

#define _Class _EditorView

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	EditorView *this = (EditorView *) self;

	release(this->bumpSlider);
	release(this->hardnessSlider);
	release(this->specularSlider);
	release(this->parallaxSlider);

	release(this->panel);

	super(Object, self, dealloc);
}

#pragma mark - Actions & Delegates

/**
 * @brief ActionFunction for the Save Button.
 */
static void saveAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	EditorView *this = (EditorView *) sender;

	printf("Save!\n");

	printf("Material %s now has bump set to %f\n", this->material->base, this->material->bump);
	printf("Material %s now has hardness set to %f\n", this->material->base, this->material->hardness);
	printf("Material %s now has specular set to %f\n", this->material->base, this->material->specular);
	printf("Material %s now has parallax set to %f\n", this->material->base, this->material->parallax);
}

/**
 * @brief SliderDelegate callback for changing bump.
 */
static void didSetValue(Slider *slider) {

	EditorView *view = (EditorView *) slider->delegate.self;

	if (!view->material) {
		return;
	}

	if (slider == view->bumpSlider) {
		view->material->bump = view->bumpSlider->value;
	} else if (slider == view->hardnessSlider) {
		view->material->hardness = view->hardnessSlider->value;
	} else if (slider == view->specularSlider) {
		view->material->specular = view->specularSlider->value;
	} else if (slider == view->parallaxSlider) {
		view->material->parallax = view->parallaxSlider->value;
	} else {
		Com_Debug(DEBUG_UI, "Unknown Slider %p\n", slider);
	}
}

#pragma mark - View

/**
 * @see View::updateBindings(View *)
 */
static void updateBindings(View *self) {

	EditorView *this = (EditorView *) self;

	vec3_t end;

	VectorMA(r_view.origin, MAX_WORLD_DIST, r_view.forward, end);

	cm_trace_t tr = Cl_Trace(r_view.origin, end, NULL, NULL, 0, MASK_SOLID);

	if (tr.fraction < 1.0) {
		this->material = tr.surface->material;

		if (!this->material) {
			Com_Debug(DEBUG_UI, "Failed to resolve %s\n", tr.surface->name);
		}
	} else {
		this->material = NULL;
	}

	if (this->material) {
		$(this->materialName, setText, this->material->base);
		$(this->diffuseTexture, setText, this->material->diffuse);
		$(this->normalmapTexture, setText, this->material->normalmap);

		$(this->bumpSlider, setValue, (double) this->material->bump);
		$(this->hardnessSlider, setValue, (double) this->material->hardness);
		$(this->specularSlider, setValue, (double) this->material->specular);
		$(this->parallaxSlider, setValue, (double) this->material->parallax);
	}
}

#pragma mark - EditorView

/**
 * @brief
 */
void addButton(View *view, const char *title, ActionFunction function, ident sender, ident data) {

	Button *button = $(alloc(Button), initWithFrame, NULL, ControlStyleDefault);

	$(button->title, setText, title);

	$((Control *) button, addActionForEventType, SDL_MOUSEBUTTONUP, function, sender, data);

	$(view, addSubview, (View *) button);

	release(button);
}

#define INPUT_LABEL_WIDTH 140

/**
 * @brief Adds a new Label and the specified Control to the given View.
 * @remarks This function releases the Control for convenience.
 */
void addInput(View *view, const char *label, Control *control) {

	assert(view);
	assert(control);

	Input *input = $(alloc(Input), initWithFrame, NULL);
	assert(input);

	$(input, setControl, control);

	$(input->label->text, setText, label);

	input->label->view.autoresizingMask &= ~ViewAutoresizingContain;
	input->label->view.frame.w = INPUT_LABEL_WIDTH;

	$(view, addSubview, (View *) input);

	release(input);
}

/**
 * @fn EditorView *EditorView::initWithFrame(EditorView *self, const SDL_Rect *frame)
 * @memberof EditorView
 */
static EditorView *initWithFrame(EditorView *self, const SDL_Rect *frame) {

	self = (EditorView *) super(View, self, initWithFrame, frame);
	if (self) {

		self->panel = $(alloc(Panel), initWithFrame, NULL);
		assert(self->panel);

		((View *) self)->alignment = ViewAlignmentMiddleCenter;

		self->panel->stackView.view.alignment = ViewAlignmentMiddleCenter;
		self->panel->stackView.view.needsLayout = true;

		$((View *) self, addSubview, (View *) self->panel);

		StackView *columns = $(alloc(StackView), initWithFrame, NULL);

		columns->axis = StackViewAxisHorizontal;
		columns->spacing = DEFAULT_PANEL_SPACING;

		{
			StackView *column = $(alloc(StackView), initWithFrame, NULL);
			column->spacing = DEFAULT_PANEL_SPACING;

			{
				Box *box = $(alloc(Box), initWithFrame, NULL);
				$(box->label, setText, "MATERIAL");


				StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

				// material name

				self->materialName = $(alloc(Text), initWithText, "<name>", NULL);

				addInput((View *) stackView, "Material name", (Control *) self->materialName);

				// diffuse texture

				self->diffuseTexture = $(alloc(Text), initWithText, "<diffuse>", NULL);

				addInput((View *) stackView, "Diffuse texture", (Control *) self->diffuseTexture);

				// normalmap texture

				self->normalmapTexture = $(alloc(Text), initWithText, "<normalmap>", NULL);

				addInput((View *) stackView, "Normalmap texture", (Control *) self->normalmapTexture);

				// bump

				self->bumpSlider = $(alloc(Slider), initWithFrame, NULL, ControlStyleDefault);

				self->bumpSlider->delegate.self = (ident *) self;
				self->bumpSlider->delegate.didSetValue = didSetValue;

				self->bumpSlider->min = 0.0;
				self->bumpSlider->max = 20.0;
				self->bumpSlider->step = 0.125;

				addInput((View *) stackView, "Bump", (Control *) self->bumpSlider);

				// hardness

				self->hardnessSlider = $(alloc(Slider), initWithFrame, NULL, ControlStyleDefault);

				self->hardnessSlider->delegate.self = (ident *) self;
				self->hardnessSlider->delegate.didSetValue = didSetValue;

				self->hardnessSlider->min = 0.0;
				self->hardnessSlider->max = 20.0;
				self->hardnessSlider->step = 0.1;

				addInput((View *) stackView, "Hardness", (Control *) self->hardnessSlider);

				// specular

				self->specularSlider = $(alloc(Slider), initWithFrame, NULL, ControlStyleDefault);

				self->specularSlider->delegate.self = (ident *) self;
				self->specularSlider->delegate.didSetValue = didSetValue;

				self->specularSlider->min = 0.0;
				self->specularSlider->max = 20.0;
				self->specularSlider->step = 0.1;

				addInput((View *) stackView, "Specular", (Control *) self->specularSlider);

				// parallax

				self->parallaxSlider = $(alloc(Slider), initWithFrame, NULL, ControlStyleDefault);

				self->parallaxSlider->delegate.self = (ident *) self;
				self->parallaxSlider->delegate.didSetValue = didSetValue;

				self->parallaxSlider->min = 0.0;
				self->parallaxSlider->max = 20.0;
				self->parallaxSlider->step = 0.1;

				addInput((View *) stackView, "Parallax", (Control *) self->parallaxSlider);

				$((View *) box, addSubview, (View *) stackView);
				release(stackView);

				$((View *) column, addSubview, (View *) box);
				release(box);
			}

			$((View *) columns, addSubview, (View *) column);
			release(column);
		}

		$((View *) self->panel->contentView, addSubview, (View *) columns);
		release(columns);

		self->panel->accessoryView->view.hidden = false;
		addButton((View *) ((EditorView *) self)->panel->accessoryView, "Save", saveAction, self, NULL);

		$((View *) self, updateBindings);

//		$((View *) self, addSubview, (View *) self->panel);
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((ViewInterface *) clazz->def->interface)->updateBindings = updateBindings;

	((EditorViewInterface *) clazz->def->interface)->initWithFrame = initWithFrame;
}

/**
 * @fn Class *EditorView::_EditorView(void)
 * @memberof EditorView
 */
Class *_EditorView(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "EditorView";
		clazz.superclass = _View();
		clazz.instanceSize = sizeof(EditorView);
		clazz.interfaceOffset = offsetof(EditorView, interface);
		clazz.interfaceSize = sizeof(EditorViewInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
