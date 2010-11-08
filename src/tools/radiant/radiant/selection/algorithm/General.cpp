#include "General.h"

#include <stack>
#include "iselection.h"
#include "iundo.h"
#include "scenelib.h"

#include "../../brush/brushmanip.h"
#include "../../map/map.h"

#include "SelectionPolicies.h"
#include "Shader.h"
#include "../SceneWalkers.h"
#include "../../select.h"

namespace selection {
	namespace algorithm {

/**
 Loops over all selected brushes and stores their
 world AABBs in the specified array.
 */
class CollectSelectedBrushesBounds: public SelectionSystem::Visitor
{
		AABB* m_bounds; // array of AABBs
		Unsigned m_max; // max AABB-elements in array
		Unsigned& m_count;// count of valid AABBs stored in array

	public:
		CollectSelectedBrushesBounds (AABB* bounds, Unsigned max, Unsigned& count) :
			m_bounds(bounds), m_max(max), m_count(count)
		{
			m_count = 0;
		}

		void visit (scene::Instance& instance) const
		{
			ASSERT_MESSAGE(m_count <= m_max, "Invalid m_count in CollectSelectedBrushesBounds");

			// stop if the array is already full
			if (m_count == m_max)
				return;

			Selectable* selectable = Instance_getSelectable(instance);
			if ((selectable != 0) && instance.isSelected()) {
				// brushes only
				if (Instance_getBrush(instance) != 0) {
					m_bounds[m_count] = instance.worldAABB();
					++m_count;
				}
			}
		}
};

/**
 Selects all objects that intersect one of the bounding AABBs.
 The exact intersection-method is specified through TSelectionPolicy
 */
template<class TSelectionPolicy>
class SelectByBounds: public scene::Graph::Walker
{
		AABB* m_aabbs; // selection aabbs
		Unsigned m_count; // number of aabbs in m_aabbs
		TSelectionPolicy policy; // type that contains a custom intersection method aabb<->aabb

	public:
		SelectByBounds (AABB* aabbs, Unsigned count) :
			m_aabbs(aabbs), m_count(count)
		{
		}

		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			Selectable* selectable = Instance_getSelectable(instance);

			/* ignore worldspawn */
			Entity* entity = Node_getEntity(path.top());
			if (entity) {
				if (string_equal(entity->getKeyValue("classname"), "worldspawn"))
					return true;
			}

			if (path.size() > 1 && !path.top().get().isRoot() && selectable != 0) {
				for (Unsigned i = 0; i < m_count; ++i) {
					if (policy.Evaluate(m_aabbs[i], instance)) {
						selectable->setSelected(true);
					}
				}
			}

			return true;
		}

		/**
		 Performs selection operation on the global scenegraph.
		 If delete_bounds_src is true, then the objects which were
		 used as source for the selection aabbs will be deleted.
		 */
		static void DoSelection (bool delete_bounds_src = true)
		{
			if (GlobalSelectionSystem().Mode() == SelectionSystem::ePrimitive) {
				// we may not need all AABBs since not all selected objects have to be brushes
				const Unsigned max = (Unsigned) GlobalSelectionSystem().countSelected();
				AABB* aabbs = new AABB[max];

				Unsigned count;
				CollectSelectedBrushesBounds collector(aabbs, max, count);
				GlobalSelectionSystem().foreachSelected(collector);

				// nothing usable in selection
				if (!count) {
					delete[] aabbs;
					return;
				}

				// delete selected objects
				if (delete_bounds_src) { // see deleteSelection
					UndoableCommand undo("deleteSelected");
					deleteSelection();
				}

				// select objects with bounds
				GlobalSceneGraph().traverse(SelectByBounds<TSelectionPolicy> (aabbs, count));

				SceneChangeNotify();
				delete[] aabbs;
			}
		}
};

void selectInside (void)
{
	SelectByBounds<SelectionPolicy_Inside>::DoSelection();
}

void selectTouching (void)
{
	SelectByBounds<SelectionPolicy_Touching>::DoSelection(false);
}

class DeleteSelected: public scene::Graph::Walker
{
		mutable bool m_remove;
		mutable bool m_removedChild;
	public:
		DeleteSelected () :
			m_remove(false), m_removedChild(false)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			m_removedChild = false;

			Selectable* selectable = Instance_getSelectable(instance);
			if (selectable != 0 && selectable->isSelected() && path.size() > 1 && !path.top().get().isRoot()) {
				m_remove = true;
				/* don't traverse into child elements */
				return false;
			}
			return true;
		}
		void post (const scene::Path& path, scene::Instance& instance) const
		{

			if (m_removedChild) {
				m_removedChild = false;

				// delete empty entities
				Entity* entity = Node_getEntity(path.top());
				if (entity != 0 && path.top().get_pointer() != Map_FindWorldspawn(g_map) && Node_getTraversable(
						path.top())->empty()) {
					Path_deleteTop(path);
				}
			}

			// node should be removed
			if (m_remove) {
				if (Node_isEntity(path.parent()) != 0) {
					m_removedChild = true;
				}

				m_remove = false;
				Path_deleteTop(path);
			}
		}
};

void deleteSelection (void)
{
	GlobalSceneGraph().traverse(DeleteSelected());
	SceneChangeNotify();
}

class InvertSelectionWalker: public scene::Graph::Walker
{
		SelectionSystem::EMode m_mode;
		mutable Selectable* m_selectable;
	public:
		InvertSelectionWalker (SelectionSystem::EMode mode) :
			m_mode(mode), m_selectable(0)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			Selectable* selectable = Instance_getSelectable(instance);
			if (selectable) {
				switch (m_mode) {
				case SelectionSystem::eEntity:
					if (Node_isEntity(path.top()) != 0) {
						m_selectable = path.top().get().visible() ? selectable : 0;
					}
					break;
				case SelectionSystem::ePrimitive:
					m_selectable = path.top().get().visible() ? selectable : 0;
					break;
				case SelectionSystem::eComponent:
					// Check if we have a componentselectiontestable instance
					ComponentSelectionTestable* compSelTestable = Instance_getComponentSelectionTestable(instance);

					// Only add it to the list if the instance has components and is already selected
					if (compSelTestable && selectable->isSelected()) {
						m_selectable = path.top().get().visible() ? selectable : 0;
					}
					break;
				}
			}
			return true;
		}
		void post (const scene::Path& path, scene::Instance& instance) const
		{
			if (m_selectable != 0) {
				m_selectable->invertSelected();
				m_selectable = 0;
			}
		}
};

void invertSelection (void)
{
	GlobalSceneGraph().traverse(InvertSelectionWalker(GlobalSelectionSystem().Mode()));
}

	} // namespace algorithm
} // namespace selection
